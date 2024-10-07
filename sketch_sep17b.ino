#define DECODE_NEC  

#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <IRremote.hpp>

const char* ssid = "TP-LINK_B383";
const char* password = "cda619872";
const char* mqttServer = "192.168.0.101";
const int mqttPort = 1883;
const char* espClientName = "w1a";

WiFiClient espClient;
PubSubClient client(espClient);

const int blueLedPin = 2;
const int redLedPin = 21;
const int yellowLedPin = 32;
const int greenLedPin = 33;
const int buttonPin = 26;

enum Color { RED, YELLOW, GREEN, OFF, PENDING };
Color currentColor = RED;


bool buttonPressed = false;


unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000;

const int RECV_PIN = 18;
IRrecv irrecv(RECV_PIN);
decode_results results;

void setup() {
  pinMode(blueLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);
  setupWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  EEPROM.begin(1);
  currentColor = (Color)EEPROM.read(0);
  setColor(currentColor);

  IrReceiver.begin(RECV_PIN);
  Serial.print(F("Ready to receive IR signals of protocols: "));
  printActiveIRProtocols(&Serial);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  digitalWrite(blueLedPin, WiFi.status() == WL_CONNECTED ? HIGH : LOW);

  handleButtonPress();
  handleIRReception();

  if (currentColor == PENDING) {
    blinkPending();
  }

  // Regelmäßiges Senden des Status
  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= sendInterval) {
    lastSendTime = currentMillis;
    sendCurrentState();
  }
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(blueLedPin, !digitalRead(blueLedPin)); // Blink LED
    delay(500);
  }
  Serial.println("Connected to WiFi");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESPClient")) {
      Serial.println("connected");
      client.subscribe(espClientName);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = String((char*)payload).substring(0, length);
  Serial.print("Message received: ");
  Serial.println(message);
  

  if (message == "red") {
    setColor(RED);
  } else if (message == "yellow") {
    setColor(YELLOW);
  } else if (message == "green") {
    setColor(GREEN);
  } else if (message == "off") {
    setColor(OFF);
  } else if (message == "pending") {
    setColor(PENDING);
  }

}

void handleButtonPress() {
  if (currentColor != OFF && currentColor != PENDING) {
    if (digitalRead(buttonPin) == LOW) {
      if (!buttonPressed) {
        switchColor(true);
        buttonPressed = true;
        delay(300); // Debounce
      }
    } else {
      buttonPressed = false;
    }
  }
}

void handleIRReception() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      Serial.print("IR command: ");
      Serial.println(IrReceiver.decodedIRData.command);
      if (IrReceiver.decodedIRData.command == 0x13) {
        if (currentColor != OFF && currentColor != PENDING) {
          switchColor(true);
          delay(1000); // Debounce
        }
      }
    }
    IrReceiver.resume();
  }
}

void switchColor(bool canSendMessage) {
  Color newColor = (currentColor == RED) ? YELLOW : (currentColor == YELLOW) ? GREEN : RED;
  setColor(newColor);
      // Nur senden, wenn sich der Zustand geändert hat und nicht von einem MQTT-Update
    if (canSendMessage) {
      sendCurrentState();

    }
}

void setColor(Color color) {
  if (currentColor != color) {
    currentColor = color;

    switch (color) {
      case RED:
        digitalWrite(redLedPin, HIGH);
        digitalWrite(yellowLedPin, LOW);
        digitalWrite(greenLedPin, LOW);
        break;
      case YELLOW:
        digitalWrite(redLedPin, LOW);
        digitalWrite(yellowLedPin, HIGH);
        digitalWrite(greenLedPin, LOW);
        break;
      case GREEN:
        digitalWrite(redLedPin, LOW);
        digitalWrite(yellowLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        break;
      case OFF:
        digitalWrite(redLedPin, LOW);
        digitalWrite(yellowLedPin, LOW);
        digitalWrite(greenLedPin, LOW);
        break;
      case PENDING:
        blinkPending();
        return; // Don't send state for pending
    }

    EEPROM.write(0, currentColor);
    EEPROM.commit();


  }
}

void blinkPending() {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = LOW;

  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= 500) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;

    digitalWrite(redLedPin, ledState ? HIGH : LOW);
    digitalWrite(yellowLedPin, ledState ? LOW : HIGH);
    digitalWrite(greenLedPin, LOW);
  }
}

void sendCurrentState() {
  String message;
  switch (currentColor) {
    case RED: message = "red"; break;
    case YELLOW: message = "yellow"; break;
    case GREEN: message = "green"; break;
    case OFF: message = "off"; break;
    case PENDING: message = "pending"; break;
  }
  client.publish(espClientName, message.c_str());
}
