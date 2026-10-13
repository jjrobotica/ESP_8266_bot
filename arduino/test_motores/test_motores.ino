// Test de motores - shield L293D para NodeMCU
// Monitor Serie a 115200 y mandá:
//   1 = Motor A con DIR HIGH     2 = Motor A con DIR LOW
//   3 = Motor B con DIR HIGH     4 = Motor B con DIR LOW
//   a = secuencia completa       0 = stop
// Cada prueba dura 2 s, frena sola y hace una pausa antes de cambiar de sentido.
// Lo esperado: cada motor gira en un sentido con HIGH y en el contrario con LOW.

#include <ESP8266WiFi.h>

#define MOTOR_A_PWM D1  // GPIO5
#define MOTOR_B_PWM D2  // GPIO4
#define MOTOR_A_DIR D3  // GPIO0
#define MOTOR_B_DIR D4  // GPIO2 (el LED azul del ESP se prende cuando está en LOW)

const int POTENCIA = 80;               // % (analogWriteRange = 100)
const unsigned long DURACION_MS = 2000;
const unsigned long PAUSA_MS = 500;    // pausa entre pruebas (no invertir en caliente)

void parar() {
  analogWrite(MOTOR_A_PWM, 0);
  analogWrite(MOTOR_B_PWM, 0);
}

void probar(const char* motor, int pinPwm, int pinDir, int nivelDir) {
  parar();
  delay(PAUSA_MS);
  digitalWrite(pinDir, nivelDir);
  Serial.printf("> Motor %s con DIR %s ... ", motor, nivelDir == HIGH ? "HIGH" : "LOW ");
  analogWrite(pinPwm, POTENCIA);
  delay(DURACION_MS);
  parar();
  Serial.println("stop");
}

void secuencia() {
  Serial.println("\n=== Secuencia completa ===");
  probar("A", MOTOR_A_PWM, MOTOR_A_DIR, HIGH);
  probar("A", MOTOR_A_PWM, MOTOR_A_DIR, LOW);
  probar("B", MOTOR_B_PWM, MOTOR_B_DIR, HIGH);
  probar("B", MOTOR_B_PWM, MOTOR_B_DIR, LOW);
  Serial.println("=== Fin ===\n");
}

void menu() {
  Serial.println("\nTest de motores L293D");
  Serial.println("1: A DIR HIGH | 2: A DIR LOW | 3: B DIR HIGH | 4: B DIR LOW");
  Serial.println("a: secuencia  | 0: stop");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);
  analogWriteRange(100);
  pinMode(MOTOR_A_PWM, OUTPUT);
  pinMode(MOTOR_B_PWM, OUTPUT);
  pinMode(MOTOR_A_DIR, OUTPUT);
  pinMode(MOTOR_B_DIR, OUTPUT);
  parar();
  delay(500);
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '1': probar("A", MOTOR_A_PWM, MOTOR_A_DIR, HIGH); break;
    case '2': probar("A", MOTOR_A_PWM, MOTOR_A_DIR, LOW);  break;
    case '3': probar("B", MOTOR_B_PWM, MOTOR_B_DIR, HIGH); break;
    case '4': probar("B", MOTOR_B_PWM, MOTOR_B_DIR, LOW);  break;
    case 'a': secuencia(); break;
    case '0': parar(); Serial.println("stop"); break;
    case '\n': case '\r': break;
    default: menu();
  }
}
