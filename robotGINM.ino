// ============================================================
//  ROBOT ESQUIVADOR v3 -
//  v1 = demasiado lento/indeciso
//  v2 = suicida (RIP)
//  v3 = ágil pero con instinto de supervivencia
// ============================================================

#define IN1 7
#define IN2 6
#define ENA 9

#define IN3 5
#define IN4 4
#define ENB 10

#define TRIG 12
#define ECHO 13

// ============================================================
//  PARÁMETROS — 
// ============================================================

// Distancias (cm)
const int DIST_LIBRE   = 45;  // Por encima: velocidad máxima, recto
const int DIST_FRENAR  = 30;  // Frenado progresivo, sin curvas al azar
const int DIST_EVASION = 22;  // Aquí esquiva — margen seguro real

// Velocidades
const int VEL_MAX  = 190;  // Un poco menos que v2, más controlable
const int VEL_MED  = 120;  // Zona de frenado
const int VEL_MIN  = 80;   // Justo antes de evadir
const int VEL_GIRO = 155;  // Giro de evasión — más suave que v2

// Tiempos de maniobra (ms)
// v1=350, v2=180 → v3=240: punto medio probado
const int T_RETROCESO    = 200;  // Retroceso real antes de escanear
const int T_ESCANEO_LADO = 240;  // Tiempo por lado de escaneo
const int T_GIRO_FINAL   = 380;  // Giro de salida

// Intervalo del sensor
const int INTERVALO_SENSOR = 45;

// ============================================================
//  ESTADO
// ============================================================

enum Estado {
  AVANZANDO,
  RETROCEDIENDO,
  ESCANEANDO_DERECHA,
  ESCANEANDO_IZQUIERDA,
  DECIDIENDO,
  GIRANDO_EVASION
};

Estado estadoActual = AVANZANDO;

unsigned long tiempoAnteriorSensor = 0;
unsigned long tiempoInicioManiobra = 0;
unsigned long tiempoActual         = 0;

float distActual            = 0;
float distEscaneo_Derecha   = 0;
float distEscaneo_Izquierda = 0;
bool  girarDerecha          = true;

// Contador de colisiones inminentes seguidas (anti-loop)
int intentosFallidos = 0;

// ============================================================
//  SETUP
// ============================================================
void setup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);

  Serial.begin(9600);
  Serial.println("=== v3: El ratón equilibrado. Sin prisa, sin pausa. ===");
  detener();
  delay(1000);
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  tiempoActual = millis();

  // Lectura periódica no bloqueante
  if (tiempoActual - tiempoAnteriorSensor >= INTERVALO_SENSOR) {
    tiempoAnteriorSensor = tiempoActual;
    distActual = medirDistancia();

    if (estadoActual == AVANZANDO) {
      manejarAvance(distActual);
    }
  }

  // Máquina de estados para evasión
  switch (estadoActual) {

    case AVANZANDO:
      // manejarAvance() ya controla la velocidad.
      // Solo disparamos evasión si llegamos al límite crítico.
      if (distActual <= DIST_EVASION) {
        Serial.println("[!] Obstáculo cercano. Iniciando evasión.");
        intentosFallidos = 0;
        iniciarRetroceso();
      }
      break;

    case RETROCEDIENDO:
      if (tiempoActual - tiempoInicioManiobra >= T_RETROCESO) {
        Serial.println("[>] Escaneando derecha...");
        estadoActual = ESCANEANDO_DERECHA;
        girarDerecha_Maniobra(VEL_GIRO);
        tiempoInicioManiobra = tiempoActual;
      }
      break;

    case ESCANEANDO_DERECHA:
      if (tiempoActual - tiempoInicioManiobra >= T_ESCANEO_LADO) {
        distEscaneo_Derecha = medirDistancia();
        Serial.print("[D] "); Serial.print(distEscaneo_Derecha); Serial.println(" cm");
        estadoActual = ESCANEANDO_IZQUIERDA;
        girarIzquierda_Maniobra(VEL_GIRO);
        tiempoInicioManiobra = tiempoActual;
      }
      break;

    case ESCANEANDO_IZQUIERDA:
      if (tiempoActual - tiempoInicioManiobra >= T_ESCANEO_LADO * 2) {
        distEscaneo_Izquierda = medirDistancia();
        Serial.print("[I] "); Serial.print(distEscaneo_Izquierda); Serial.println(" cm");
        estadoActual = DECIDIENDO;
      }
      break;

    case DECIDIENDO: {
      bool ambosBlockeados = (distEscaneo_Derecha  < DIST_EVASION &&
                              distEscaneo_Izquierda < DIST_EVASION);

      if (ambosBlockeados) {
        intentosFallidos++;
        Serial.print("[!!] Ambos bloqueados. Intento #");
        Serial.println(intentosFallidos);

        // Tras 3 intentos fallidos: retroceso largo de emergencia
        if (intentosFallidos >= 3) {
          Serial.println("[SOS] Retroceso de emergencia prolongado.");
          retroceder(VEL_MED);
          delay(600); // Único delay permitido: emergencia real
          intentosFallidos = 0;
        }
        iniciarRetroceso();
        break;
      }

      intentosFallidos = 0;
      girarDerecha = (distEscaneo_Derecha >= distEscaneo_Izquierda);
      Serial.print("[✓] Giro hacia: ");
      Serial.println(girarDerecha ? "DERECHA" : "IZQUIERDA");

      if (girarDerecha) girarDerecha_Maniobra(VEL_GIRO);
      else              girarIzquierda_Maniobra(VEL_GIRO);

      estadoActual = GIRANDO_EVASION;
      tiempoInicioManiobra = tiempoActual;
      break;
    }

    case GIRANDO_EVASION:
      if (tiempoActual - tiempoInicioManiobra >= T_GIRO_FINAL) {
        // Verifica que el camino esté despejado ANTES de avanzar
        // (lección aprendida de la v2)
        float distTrasGiro = medirDistancia();
        if (distTrasGiro > DIST_EVASION) {
          Serial.println("[→] Camino libre. Avanzando.");
          estadoActual = AVANZANDO;
          avanzar(VEL_MAX);
        } else {
          // Aún bloqueado: girar un poco más
          Serial.println("[?] Aún bloqueado tras giro. Continuando...");
          tiempoInicioManiobra = tiempoActual; // Extiende el giro
        }
      }
      break;
  }
}

// ============================================================
//  MANEJO DE AVANCE CON FRENADO PROGRESIVO
//  Sin curvas al azar (eso mató al ratón anterior)
// ============================================================
void manejarAvance(float distancia) {
  if (distancia > DIST_LIBRE) {
    avanzar(VEL_MAX);

  } else if (distancia > DIST_FRENAR) {
    // Frenado suave proporcional
    int vel = map((int)distancia, DIST_FRENAR, DIST_LIBRE, VEL_MED, VEL_MAX);
    vel = constrain(vel, VEL_MED, VEL_MAX);
    avanzar(vel);

  } else if (distancia > DIST_EVASION) {
    // Frenado fuerte — ya casi en zona de evasión
    int vel = map((int)distancia, DIST_EVASION, DIST_FRENAR, VEL_MIN, VEL_MED);
    vel = constrain(vel, VEL_MIN, VEL_MED);
    avanzar(vel);
  }
  // Si <= DIST_EVASION, el switch lo maneja
}

// ============================================================
//  HELPER
// ============================================================
void iniciarRetroceso() {
  estadoActual = RETROCEDIENDO;
  retroceder(VEL_MIN);
  tiempoInicioManiobra = tiempoActual;
}

// ============================================================
//  MOVIMIENTO — Motor derecho invertido (Taco Rojo corregido)
// ============================================================
void avanzar(int vel) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENA, vel);   analogWrite(ENB, vel);
}

void retroceder(int vel) {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, vel);   analogWrite(ENB, vel);
}

void girarDerecha_Maniobra(int vel) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, vel);   analogWrite(ENB, vel);
}

void girarIzquierda_Maniobra(int vel) {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENA, vel);   analogWrite(ENB, vel);
}

void detener() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
}

// ============================================================
//  SENSOR HC-SR04
// ============================================================
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long dur = pulseIn(ECHO, HIGH, 25000);
  if (dur == 0) return 400.0;
  return (dur * 0.0343) / 2.0;
}
