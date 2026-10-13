# futbolista_esp8266 (AutitoJJ)

Robot "futbolista" de dos motores controlado desde el celular. Un ESP8266 levanta su propio WiFi (modo AP) y un servidor WebSocket; una interfaz web con botones táctiles le manda los comandos de movimiento.

## Características
- Punto de acceso WiFi autónomo: red `AutitoJJ`, clave `12345678`.
- Control de dos motores DC con driver L293D (shield de motores para NodeMCU).
- Interfaz web táctil en horizontal, con multitouch: avanzar, retroceder, girar y curvas combinando botones.
- Panel de ajustes (tocando el logo) con modos de prueba de curva y giro, y sliders para ajustar la potencia (0–100 %). Los valores se guardan en el navegador y se reenvían al reconectar.
- Protección del driver: al invertir el sentido corta la potencia y espera 150 ms antes de cambiar, y acelera en rampa. `VEL_MAX` en el firmware limita la potencia máxima.
- Un solo cliente a la vez: si entra otro, el ESP8266 lo rechaza. Si el cliente se desconecta, los motores se detienen.

## Estructura del repositorio
```
├── arduino/
│   └── esp8266/
│       └── esp8266.ino   # Firmware del ESP8266
├── Web/                  # Interfaz web estática
│   ├── index.html
│   ├── estilo.css
│   └── img/
└── README.md
```

## Hardware
- NodeMCU / ESP-12E (ESP8266) con shield de motores L293D.
- Dos motores DC y alimentación para motores.

| Señal | Pin NodeMCU | GPIO |
|---|---|---|
| PWM motor A | D1 | 5 |
| PWM motor B | D2 | 4 |
| Dirección motor A | D3 | 0 |
| Dirección motor B | D4 | 2 |

> Si el robot gira o retrocede cuando debería avanzar, invertí los cables de ese motor (o cambiá HIGH/LOW de su dirección en el firmware).

## Software
- [Arduino IDE](https://www.arduino.cc/en/software) con el core ESP8266.
- Librerías:
  - `ESPAsyncTCP`
  - `ESPAsyncWebServer`
  - (`ESP8266WiFi` viene con el core)

## Instalación y uso
1. Abrí `arduino/esp8266/esp8266.ino` en el Arduino IDE, instalá las librerías y cargalo en el ESP8266.
2. Conectá el celular al WiFi `AutitoJJ` (clave `12345678`).
3. Abrí `Web/index.html` en el celular. El firmware **no** sirve la página: la web se conecta directo a `ws://192.168.4.1/ws`. Está preparada para correr dentro de una app de App Inventor con un WebView (usa `window.AppInventor`, por ejemplo tocando el título), pero también funciona en un navegador si los archivos de `Web/` están en el dispositivo.
4. Poné el celular en horizontal y manejá con las flechas. El estado de conexión aparece abajo a la derecha.

## Protocolo WebSocket
| Comando | Acción |
|---|---|
| `F` / `B` | Avanzar / retroceder |
| `L` / `R` | Giro en el lugar izquierda / derecha |
| `F,L` / `F,R` | Curva hacia adelante izquierda / derecha |
| `B,L` / `B,R` | Curva hacia atrás izquierda / derecha |
| `S` | Detener |
| `CI` / `CD` | Modo prueba: curva izquierda / derecha continua |
| `GI` / `GD` | Modo prueba: giro izquierda / derecha continuo |
| `PWM_curva,<0-100>` | Potencia de la rueda lenta en las curvas |
| `PWM_giro,<0-100>` | Potencia del giro en el lugar |

La web manda el comando cuando cambia y, además, lo repite cada 200 ms como latido. Si el ESP8266 no recibe nada durante 600 ms (se cortó el WiFi, se cerró la app, etc.), detiene los motores. Si un cliente nuevo se conecta, reemplaza al anterior.

Para comprobar la conexión sin la web: entrá a `http://192.168.4.1` desde el celular; tiene que responder `AutitoJJ OK`.

## Contribuciones
Las contribuciones y mejoras son bienvenidas. Si encontrás un problema o tenés una idea, abrí un issue o mandá un pull request.
