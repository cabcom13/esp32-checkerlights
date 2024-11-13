#define DECODE_NEC

#include <WiFi.h>
#include <PubSubClient.h>
#include <IRremote.hpp>

const char* ssid = "TP-LINK_B383";
const char* password = "";
const char* mqttServer = "192.168.0.101";
const int mqttPort = 1883;

const char* espClientName = "w1a";
const int IRCOMMAND = 0x9;

WiFiClient espClient;
PubSubClient client(espClient);

const int redLedPin = 15;
const int BlueLedPin = 5;
const int greenLedPin = 4;

enum Color {
  RED,
  YELLOW,
  GREEN,
  BLUE,
  CYAN,
  MAGENTA,
  WHITE,
  OFF,
  PENDING
};
Color currentColor = RED;

bool buttonPressed = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000;

const int RECV_PIN = 18;
IRrecv irrecv(RECV_PIN);
decode_results results;

void setup() {
  Serial.begin(115200);
  setupWiFi();
  pinMode(redLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  IrReceiver.begin(RECV_PIN);
  sendCurrentState();  // Send Heartbeat
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  handleIRReception();

  if (currentColor == PENDING) {
    blinkPending();
  }

  // Regular status update
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    sendCurrentState();
  }
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("Connected to WiFi!");
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP_" + String(espClientName);
    if (client.connect(clientId.c_str())) {
      Serial.println(clientId + " connected!");
      client.subscribe(espClientName);
      client.subscribe("all");

      setColor(GREEN);
      // sendCurrentState();  // Send Heartbeat
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      setColor(RED);
      // sendCurrentState();  // Send Heartbeat
      delay(5000);  // Retry every 5 seconds
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = String((char*)payload).substring(0, length);
  Serial.println(message);

  if (String(topic) == espClientName || String(topic) == "all") {
    if (message == "red") setColor(RED);
    else if (message == "blue") setColor(BLUE);
    else if (message == "green") setColor(GREEN);
    else if (message == "yellow") setColor(YELLOW);
    else if (message == "cyan") setColor(CYAN);
    else if (message == "magenta") setColor(MAGENTA);
    else if (message == "white") setColor(WHITE);
    else if (message == "off") setColor(OFF);
    else if (message == "pending") setColor(PENDING);

  }
}

void handleIRReception() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN &&
        IrReceiver.decodedIRData.command == IRCOMMAND) {

      if (currentColor != OFF && currentColor != PENDING && currentColor != CYAN && currentColor != MAGENTA  && currentColor != BLUE) {
          analogWrite(redLedPin, 255);
          analogWrite(BlueLedPin, 180);
          analogWrite(greenLedPin, 255);
          delay(100);
          setColor(currentColor);  
        switchColor();
      }
    }
    IrReceiver.resume();
  }
}

void switchColor() {
  // Color newColor = (currentColor == RED) ? YELLOW : RED;
  // setColor(newColor);
  
  // Wenn die aktuelle Farbe grün ist, wechsle zu weiß
  if (currentColor == GREEN) {
    setColor(WHITE);  // Von Grün nach Weiß
  }
  // Wenn die aktuelle Farbe rot ist, wechsle zu gelb
  else if (currentColor == RED) {
    setColor(YELLOW);  // Von Rot nach Gelb
  }
  // Wenn die aktuelle Farbe gelb ist, wechsle zu rot
  else if (currentColor == YELLOW) {
    setColor(RED);  // Von Gelb nach Rot
  }
  else if (currentColor == WHITE) {
    setColor(RED);  // Von Gelb nach Rot
  }
  sendCurrentState();
}

void fadeToColor(Color targetColor) {
  const int fadeSpeed = 10;
  const int steps = 100;  // Anzahl der Übergangsschritte
  int redStart, greenStart, blueStart;
  int redEnd, greenEnd, blueEnd;

  // Startwerte der aktuellen Farbe
  getRGBValues(currentColor, redStart, greenStart, blueStart);
  
  // Endwerte der Ziel-Farbe
  getRGBValues(targetColor, redEnd, greenEnd, blueEnd);

  // Übergang in kleinen Schritten (Interpolation der RGB-Werte)
  for (int i = 0; i <= steps; i++) {
    int redValue = redStart + ((redEnd - redStart) * i) / steps;
    int greenValue = greenStart + ((greenEnd - greenStart) * i) / steps;
    int blueValue = blueStart + ((blueEnd - blueStart) * i) / steps;

    // Setze die RGB-Werte
    setPWMColor(redValue, greenValue, blueValue);

    delay(fadeSpeed);  // Schrittgeschwindigkeit
  }

  currentColor = targetColor;  // Aktualisieren der aktuellen Farbe
}

void setColor(Color color) {
  if (currentColor != color) {
    fadeToColor(color);
  }
}

void setPWMColor(int red, int green, int blue) {
  analogWrite(redLedPin, red);
  analogWrite(greenLedPin, green);
  analogWrite(BlueLedPin, blue);
}

void getRGBValues(Color color, int &red, int &green, int &blue) {
  switch (color) {
    case RED: red = 255; green = 0; blue = 0; break;
    case GREEN: red = 0; green = 255; blue = 0; break;
    case BLUE: red = 0; green = 0; blue = 255; break;
    case YELLOW: red = 255; green = 70; blue = 0; break;
    case CYAN: red = 0; green = 255; blue = 255; break;
    case MAGENTA: red = 255; green = 0; blue = 255; break;
    case WHITE: red = 255; green = 255; blue = 160; break;
    case OFF: red = 0; green = 0; blue = 0; break;
  }
}

void blinkColor(String color, unsigned long frequency) {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = LOW;
  
  if (millis() - lastBlinkTime >= frequency) {
    lastBlinkTime = millis();
    ledState = !ledState;

    if (color == "red_blink") setPWMColor(255, 0, 0);  // Nur 3 Argumente
    else if (color == "green_blink") setPWMColor(0, 255, 0);
    else if (color == "yellow_blink") setPWMColor(255, 255, 0);
    else if (color == "cyan_blink") setPWMColor(0, 255, 255);
    else if (color == "magenta_blink") setPWMColor(255, 0, 255);
    else if (color == "white_blink") setPWMColor(255, 255, 255);
  }
}

void blinkPending() {
  blinkColor("red_blink", 500);  // Red blinking for pending state
}

void sendCurrentState() {
  String message;
  switch (currentColor) {
    case RED: message = "red"; break;
    case YELLOW: message = "yellow"; break;
    case GREEN: message = "green"; break;
    case BLUE: message = "blue"; break;
    case CYAN: message = "cyan"; break;
    case MAGENTA: message = "magenta"; break;
    case WHITE: message = "white"; break;
    case OFF: message = "off"; break;
    case PENDING: message = "pending"; break;
  }
  client.publish(espClientName, message.c_str());
}
