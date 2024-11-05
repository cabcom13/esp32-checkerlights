#define DECODE_NEC

#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <IRremote.hpp>

const char* ssid = "TP-LINK_B383";
const char* password = "cda619872";
const char* mqttServer = "192.168.0.101";
const int mqttPort = 1883;
const char* espClientName = "w1b";

WiFiClient espClient;
PubSubClient client(espClient);

// const int blueLedPin = 2;
const int redLedPin = 15;
const int BlueLedPin = 5;
const int greenLedPin = 4;


const int IRCOMMAND = 0x9;
// const int IRCOMMAND = 0x10;
// const int IRCOMMAND = 0x11;
// const int IRCOMMAND = 0x12;
// const int IRCOMMAND = 0x13;
// const int IRCOMMAND = 0x14;
// const int IRCOMMAND = 0x15;
// const int IRCOMMAND = 0x16;
// const int IRCOMMAND = 0x17;
// const int IRCOMMAND = 0x18;
// const int IRCOMMAND = 0x19;
// const int IRCOMMAND = 0x20;
// const int IRCOMMAND = 0x21;
// const int IRCOMMAND = 0x22;
// const int IRCOMMAND = 0x23;


enum Color {
  RED,
  YELHIGH,
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

  pinMode(redLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);


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
    // digitalWrite(blueLedPin, !digitalRead(blueLedPin)); // Blink LED
    delay(500);
  }
  Serial.println("Connected to WiFi");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP_" + String(espClientName); // Erstelle den Client-Namen als String
    if (client.connect(clientId.c_str())) {
      Serial.println(clientId +" connected!");
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
  } else if (message == "blue") {
    setColor(BLUE);
  } else if (message == "green") {
    setColor(GREEN);
  } else if (message == "yellow") {
    setColor(YELHIGH);
  } else if (message == "cyan") {
    setColor(CYAN);
  } else if (message == "magenta") {
    setColor(MAGENTA);
  } else if (message == "white") {
    setColor(WHITE);
  } else if (message == "off") {
    setColor(OFF);
  } else if (message == "pending") {
    setColor(PENDING);
  }
}


void handleIRReception() {

  if (IrReceiver.decode()) {
    
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      Serial.print("IR command: ");
      Serial.println(IrReceiver.decodedIRData.command);

      if (IrReceiver.decodedIRData.command == IRCOMMAND) {
        if (currentColor != OFF && currentColor != PENDING && currentColor != CYAN && currentColor != MAGENTA && currentColor != WHITE && currentColor != BLUE) {
          switchColor(true);
          delay(1000);  // Debounce
        }
      }
    }
    IrReceiver.resume();
  }
}

void switchColor(bool canSendMessage) {
  Color newColor = (currentColor == RED) ? YELHIGH : (currentColor == YELHIGH) ? GREEN
                                                                             : RED;
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
        digitalWrite(BlueLedPin, LOW);
        digitalWrite(greenLedPin, LOW);
        break;
      case BLUE:
        digitalWrite(redLedPin, LOW);
        digitalWrite(BlueLedPin, HIGH);
        digitalWrite(greenLedPin, LOW);
        break;
      case GREEN:
        digitalWrite(redLedPin, LOW);
        digitalWrite(BlueLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        break;
      case YELHIGH:
        digitalWrite(redLedPin, HIGH);
        digitalWrite(BlueLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        break;
      case CYAN:
        digitalWrite(redLedPin, LOW);
        digitalWrite(BlueLedPin, HIGH);
        digitalWrite(greenLedPin, HIGH);
        break;
      case MAGENTA:
        digitalWrite(redLedPin, HIGH);
        digitalWrite(BlueLedPin, HIGH);
        digitalWrite(greenLedPin, LOW);
        break;
      case WHITE:
        digitalWrite(redLedPin, HIGH);
        digitalWrite(BlueLedPin, HIGH);
        digitalWrite(greenLedPin, HIGH);
        break;
      case OFF:
        digitalWrite(redLedPin, LOW);
        digitalWrite(BlueLedPin, LOW);
        digitalWrite(greenLedPin, LOW);
        break;
      case PENDING:
        blinkPending();
        return;  // Don't send state for pending
    }
    // switch (color) {
    //   case RED:
    //     digitalWrite(redLedPin, HIGH);
    //     digitalWrite(BlueLedPin, LOW);
    //     digitalWrite(greenLedPin, LOW);
    //     break;
    //   case YELHIGH:
    //     digitalWrite(redLedPin, HIGH);
    //     digitalWrite(BlueLedPin, LOW);
    //     digitalWrite(greenLedPin, HIGH);
    //     break;
    //   case GREEN:
    //     digitalWrite(redLedPin, LOW);
    //     digitalWrite(BlueLedPin, LOW);
    //     digitalWrite(greenLedPin, HIGH);
    //     break;
    //   case OFF:
    //     digitalWrite(redLedPin,LOW);
    //     digitalWrite(BlueLedPin, LOW);
    //     digitalWrite(greenLedPin, LOW);
    //     break;
    //   case PENDING:
    //     blinkPending();
    //     return; // Don't send state for pending
    // }

    EEPROM.write(0, currentColor);
    EEPROM.commit();
  }
}

void blinkPending() {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = HIGH;

  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= 500) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;

    digitalWrite(redLedPin, ledState ? LOW : HIGH);
    digitalWrite(BlueLedPin, ledState ? HIGH : LOW);
    digitalWrite(greenLedPin, HIGH);
  }
}

void sendCurrentState() {
  String message;
  switch (currentColor) {
    case RED: message = "red"; break;
    case YELHIGH: message = "yellow"; break;
    case GREEN: message = "green"; break;
    case OFF: message = "off"; break;
    case PENDING: message = "pending"; break;
  }
  client.publish(espClientName, message.c_str());
}
