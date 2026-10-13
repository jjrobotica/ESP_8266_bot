#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ---------- Pines shield L293D (NodeMCU) ----------
#define MOTOR_A_PWM D1  // GPIO5
#define MOTOR_B_PWM D2  // GPIO4
#define MOTOR_A_DIR D3  // GPIO0
#define MOTOR_B_DIR D4  // GPIO2
// Motor A = rueda derecha, Motor B = rueda izquierda
// Adelante: A en HIGH y B en LOW (los motores están montados espejados)

// ---------- WiFi ----------
const char* ssid = "AutitoJJ";
const char* password = "12345678";
const int CANAL_WIFI = 6;          // 1, 6 u 11: usá el que tenga menos redes alrededor
const int MAX_CLIENTES_WIFI = 1;   // solo el celular que maneja
IPAddress ipAP(192, 168, 4, 1);    // la web se conecta a esta IP
IPAddress mascara(255, 255, 255, 0);

// ---------- Failsafe ----------
// La web manda el comando actual cada 200 ms. Si no llega nada en este tiempo, frena.
const unsigned long FAILSAFE_MS = 600;
unsigned long ultimoMensajeMs = 0;
bool motoresActivos = false;

// ---------- Protección del driver ----------
const int VEL_MAX = 100;                    // tope de potencia en % (bajalo, ej. 80, para cuidar el driver)
const unsigned long PAUSA_INVERSION_MS = 150; // tiempo frenado antes de invertir el sentido
const unsigned long TICK_MS = 10;           // cada cuánto se actualizan los motores
const int PASO_RAMPA = 7;                   // % por tick al acelerar (0→100 en ~150 ms)

struct Motor {
  uint8_t pinPwm;
  uint8_t pinDir;
  int dirActual;            // sentido aplicado al driver
  int dirObjetivo;          // sentido pedido
  int pwmActual;            // potencia aplicada (0-100)
  int pwmObjetivo;          // potencia pedida (0-100)
  unsigned long ultimoActivoMs; // última vez que tuvo potencia
};

Motor motorA = { MOTOR_A_PWM, MOTOR_A_DIR, LOW, LOW, 0, 0, 0 };
Motor motorB = { MOTOR_B_PWM, MOTOR_B_DIR, LOW, LOW, 0, 0, 0 };

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
uint32_t clienteId = 0;  // 0 = ningún cliente (se guarda el id, no el puntero)

String ultimoModo = "S";

// En % (0-100), porque analogWriteRange(100)
int pwmCurva = 50;
int pwmGiro = 50;

// ---------- Control de motores ----------

// Pide un sentido y una potencia. Lo aplica actualizarMotor() de forma segura.
void fijar(Motor& m, int dir, int pwm) {
  m.dirObjetivo = dir;
  m.pwmObjetivo = constrain(pwm, 0, 100) * VEL_MAX / 100;
}

// Corte inmediato (para stop y failsafe)
void cortar(Motor& m) {
  m.pwmObjetivo = 0;
  if (m.pwmActual > 0) m.ultimoActivoMs = millis();
  m.pwmActual = 0;
  analogWrite(m.pinPwm, 0);
}

void actualizarMotor(Motor& m, const char* nombre) {
  unsigned long ahora = millis();

  if (m.pwmObjetivo == 0) {
    if (m.pwmActual > 0) m.ultimoActivoMs = ahora;
    m.pwmActual = 0;
    analogWrite(m.pinPwm, 0);
    return;
  }

  if (m.dirObjetivo != m.dirActual) {
    // 1) Si está andando para el otro lado, corta la potencia
    if (m.pwmActual > 0) {
      m.pwmActual = 0;
      analogWrite(m.pinPwm, 0);
      m.ultimoActivoMs = ahora;
      Serial.printf("↩️ Inversión protegida motor %s\n", nombre);
      return;
    }
    // 2) Espera a que se frene por inercia
    if (ahora - m.ultimoActivoMs < PAUSA_INVERSION_MS) return;
    // 3) Recién ahí cambia el sentido
    m.dirActual = m.dirObjetivo;
    digitalWrite(m.pinDir, m.dirActual);
  }

  // Acelera en rampa; desacelerar es inmediato
  if (m.pwmActual < m.pwmObjetivo) m.pwmActual = min(m.pwmObjetivo, m.pwmActual + PASO_RAMPA);
  else m.pwmActual = m.pwmObjetivo;

  analogWrite(m.pinPwm, m.pwmActual);
  if (m.pwmActual > 0) m.ultimoActivoMs = ahora;
}

// ---------- Movimientos ----------

void detener() {
  cortar(motorA);
  cortar(motorB);
  motoresActivos = false;
}

void avanzar() {
  fijar(motorA, HIGH, 100);
  fijar(motorB, LOW, 100);
}

void retroceder() {
  fijar(motorA, LOW, 100);
  fijar(motorB, HIGH, 100);
}

void giroDerecha() {
  fijar(motorA, LOW, pwmGiro);   // rueda derecha atrás
  fijar(motorB, LOW, pwmGiro);   // rueda izquierda adelante
}

void giroIzquierda() {
  fijar(motorA, HIGH, pwmGiro);  // rueda derecha adelante
  fijar(motorB, HIGH, pwmGiro);  // rueda izquierda atrás
}

void curvaDerecha() {
  fijar(motorA, HIGH, pwmCurva);
  fijar(motorB, LOW, 100);
}

void curvaIzquierda() {
  fijar(motorA, HIGH, 100);
  fijar(motorB, LOW, pwmCurva);
}

void curvaIzquierdaAtras() {
  fijar(motorA, LOW, 100);        // lado rápido
  fijar(motorB, HIGH, pwmCurva);  // lado lento
}

void curvaDerechaAtras() {
  fijar(motorA, LOW, pwmCurva);   // lado lento
  fijar(motorB, HIGH, 100);       // lado rápido
}

void interpretarComando(const String& cmd) {
  motoresActivos = true;  // detener() lo vuelve a false

  if (cmd == "F") avanzar();
  else if (cmd == "B") retroceder();
  else if (cmd == "L") giroIzquierda();
  else if (cmd == "R") giroDerecha();
  else if (cmd == "S") { detener(); ultimoModo = "S"; }
  else if (cmd == "F,L") curvaIzquierda();
  else if (cmd == "F,R") curvaDerecha();
  else if (cmd == "B,L") curvaIzquierdaAtras();
  else if (cmd == "B,R") curvaDerechaAtras();
  else if (cmd == "CD") { curvaDerecha(); ultimoModo = cmd; }
  else if (cmd == "CI") { curvaIzquierda(); ultimoModo = cmd; }
  else if (cmd == "GD") { giroDerecha(); ultimoModo = cmd; }
  else if (cmd == "GI") { giroIzquierda(); ultimoModo = cmd; }
  else if (cmd.startsWith("PWM_curva,")) {
    pwmCurva = constrain(cmd.substring(10).toInt(), 0, 100);
    Serial.print("PWM curva: "); Serial.println(pwmCurva);
    // Solo re-aplica si el modo curva está activo (evita que arranque solo al reconectar)
    if (ultimoModo == "CD") curvaDerecha();
    else if (ultimoModo == "CI") curvaIzquierda();
  }
  else if (cmd.startsWith("PWM_giro,")) {
    pwmGiro = constrain(cmd.substring(9).toInt(), 0, 100);
    Serial.print("PWM giro: "); Serial.println(pwmGiro);
    if (ultimoModo == "GD") giroDerecha();
    else if (ultimoModo == "GI") giroIzquierda();
  }
  else {
    Serial.println("Comando no reconocido: " + cmd);
    detener();
  }
}

// ---------- WebSocket ----------

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // El último que conecta gana: si el celular perdió WiFi y vuelve, la conexión
    // vieja puede seguir "viva" varios segundos y antes bloqueaba la reconexión.
    if (clienteId != 0 && clienteId != client->id()) {
      AsyncWebSocketClient* anterior = ws.client(clienteId);
      if (anterior) anterior->close();
      Serial.println("♻️ Nuevo cliente reemplaza al anterior");
    }
    detener();
    ultimoModo = "S";
    clienteId = client->id();
    ultimoMensajeMs = millis();
    Serial.printf("✅ Cliente conectado: #%u\n", client->id());
  }

  else if (type == WS_EVT_DISCONNECT) {
    if (client->id() == clienteId) {
      clienteId = 0;
      detener();
      ultimoModo = "S";
      Serial.println("🔌 Cliente desconectado");
    }
  }

  else if (type == WS_EVT_DATA) {
    if (client->id() != clienteId) return;

    // Solo mensajes de texto completos en un único frame
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)) return;

    String msg;
    msg.reserve(len);
    for (size_t i = 0; i < len; i++) msg += (char)data[i];
    msg.trim();

    ultimoMensajeMs = millis();

    // Imprime solo cuando cambia (el latido repite el mismo comando cada 200 ms)
    static String ultimoRecibido = "";
    if (msg != ultimoRecibido) {
      Serial.println("📨 " + msg);
      ultimoRecibido = msg;
    }

    interpretarComando(msg);
  }
}

void setup() {
  Serial.begin(115200);
  analogWriteRange(100);
  pinMode(MOTOR_A_PWM, OUTPUT);
  pinMode(MOTOR_A_DIR, OUTPUT);
  pinMode(MOTOR_B_PWM, OUTPUT);
  pinMode(MOTOR_B_DIR, OUTPUT);
  digitalWrite(MOTOR_A_DIR, motorA.dirActual);
  digitalWrite(MOTOR_B_DIR, motorB.dirActual);
  detener();

  WiFi.persistent(false);              // no reescribe la config WiFi en flash en cada arranque
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAPConfig(ipAP, ipAP, mascara);
  WiFi.softAP(ssid, password, CANAL_WIFI, 0, MAX_CLIENTES_WIFI);
  Serial.println("🚗 Autito en AP listo: " + WiFi.softAPIP().toString());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Para probar desde el navegador del celular: http://192.168.4.1
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "AutitoJJ OK");
  });

  server.begin();
}

void loop() {
  // Failsafe: si se corta la señal mientras se mueve, frena
  if (motoresActivos && millis() - ultimoMensajeMs > FAILSAFE_MS) {
    detener();
    ultimoModo = "S";
    Serial.println("⛔ Failsafe: sin señal, motores detenidos");
  }

  // Motores: pausa antes de invertir + rampa de aceleración
  static unsigned long ultimoTick = 0;
  if (millis() - ultimoTick >= TICK_MS) {
    ultimoTick = millis();
    actualizarMotor(motorA, "A");
    actualizarMotor(motorB, "B");
  }

  // Libera memoria de clientes WebSocket cerrados
  static unsigned long ultimaLimpieza = 0;
  if (millis() - ultimaLimpieza > 1000) {
    ws.cleanupClients();
    ultimaLimpieza = millis();
  }
}
