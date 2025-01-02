#define DECODE_NEC

#include <WiFi.h>
// #include <PubSubClient.h>
#include <MQTTClient.h>
#include <IRremote.hpp>
#include <WebOTA.h>
#include <Preferences.h>
// #include <esp_task_wdt.h>
#include <string.h>

// Watchdog Timeout in Sekunden
#define WDT_TIMEOUT 30

// WiFi und MQTT Konfiguration
// const char* ssid = "TP-LINK_B383_u1";
// const char* ssid = "TP-LINK_B383_u2-1";
const char* ssid = "TP-LINK_B383_u2-2";
// const char* ssid = "TP-LINK_B383";

const char* password = "cda619872";
const char* mqttServer = "192.168.0.101";
const int mqttPort = 1883;
const int MQTT_RETRY_DELAY = 5000;
const int MQTT_MAX_RETRIES = 3;

// const char* espClientName = "w1a";
// const char* host = "ESP-w1a";
// const int irCommand = 0x9;

// Globale Variablen
String espClientName;
String wifiSSID;
String host;
int irCommand;
int ir_tl_Command;

// Hardware-Pins
const int redLedPin = 15;
const int BlueLedPin = 5;
const int greenLedPin = 4;
const int RECV_PIN = 18;

// Objekte
WiFiClient espClient;
MQTTClient mqttClient(256);
// PubSubClient client(espClient);
IRrecv irrecv(RECV_PIN);
extern Preferences preferences;

// Status-Variablen
enum Color {
  RED,
  YELLOW,
  GREEN,
  BLUE,
  CYAN,
  MAGENTA,
  WHITE,
  OFF
};
Color currentColor = CYAN;
bool isStandby = false;
int buttonCount = 0;
unsigned long lastSendTime = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long sendInterval = 30000;
const unsigned long wifiCheckInterval = 60000;

// Watchdog Reset Funktion
void IRAM_ATTR resetModule() {
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  preferences.begin("led-state", true);
  espClientName = preferences.getString("espClientName", "default");
  wifiSSID = preferences.getString("wifiSSID", "TP-LINK_B383_u1");
  host = preferences.getString("host", "default");
  irCommand = preferences.getInt("IRCOMMAND");
  ir_tl_Command = preferences.getInt("IR_TL_COMMAND");
  preferences.end();

  Serial.println("espClientName: " + espClientName);
  Serial.println("Host: " + host);
  Serial.printf("IR TL Command: 0x%X\n", ir_tl_Command);

  // Watchdog Timer initialisieren
  // esp_task_wdt_config_t wdt_config = {
  //     .timeout_ms = WDT_TIMEOUT * 1000,  // Timeout in Millisekunden
  //     .idle_core_mask = 0,              // Watchdog wird auf allen Kernen überwacht
  //     .trigger_panic = true             // Gerät wird bei Timeout neugestartet
  // };
  // esp_task_wdt_init(&wdt_config);

  // Task zur Überwachung hinzufügen
  //  esp_task_wdt_add(NULL);

  // Preferences initialisieren
  preferences.begin("led-state", false);

  // Pins initialisieren
  pinMode(redLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  // WiFi und MQTT Setup
  setupWiFi();

  setupMQTT();
  // IR-Empfänger initialisieren
  IrReceiver.begin(RECV_PIN);

  // Letzte bekannte Farbe wiederherstellen
  // restoreLastColor();

}
void loop() {



  mqttClient.loop();
  delay(10);
  // WiFi-Verbindung prüfen
  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }

  // MQTT-Verbindung prüfen
  if (!mqttClient.connected() || WiFi.status() == WL_CONNECTED) {
    connectMQTT();

  }


  // Status-Update senden
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    if (mqttClient.connected()) {
       mqttClient.publish((espClientName + "_status").c_str(), "online", false, 0);
       Serial.println("Online");
    }
  }

  // OTA Updates behandeln
  webota.handle();

  // IR-Empfang prüfen
  handleIRReception();


  // Watchdog Reset
  //  esp_task_wdt_reset();
  //  delay(10);  // Kurze Pause, um den Watchdog zu füttern
}
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(espClientName.c_str());

  WiFi.setAutoReconnect(true);
  // WiFi.persistent(true);
  // Hostname setzen

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {

    setPWMColor(255, 0, 174);
    delay(500);
    setPWMColor(0, 0, 0);
    delay(500);
    attempts++;
    //  esp_task_wdt_reset(); // Watchdog während der Verbindungsversuche zurücksetzen
  }

  if (WiFi.status() == WL_CONNECTED) {
    attempts = 0;
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println(WiFi.macAddress());
    Serial.println(WiFi.getHostname());
    Serial.println("Signal:");
    Serial.print(WiFi.RSSI());
    delay(500);
  
  } else {
    Serial.println("\nWiFi connection failed");
    ESP.restart();
  }
}


void setupMQTT() {
  // mqttClient.setKeepAlive(120);
  mqttClient.begin(mqttServer, espClient);
  mqttClient.onMessage(messageReceived);
  const char* lwtTopic = (espClientName + "/status").c_str();
  const char* lwtMessage = "offline";

  // LWT konfigurieren
  mqttClient.setWill(lwtTopic, lwtMessage, true, 0);

  connectMQTT();
}

void connectMQTT() {
  int mqttattempts = 0;
  while (!mqttClient.connected() && mqttattempts < 5) {
    Serial.println("Connecting to MQTT... ");
    setPWMColor(0, 0, 0);
    delay(500);
    setPWMColor(0, 0, 255);
    delay(500);
    setPWMColor(0, 0, 0);
    delay(500);
    // MQTT-Verbindung herstellen
    if (mqttClient.connect(espClientName.c_str())) {
      mqttattempts = 0;
      if(isStandby == false){
        restoreLastColor();
        
      }
      Serial.println("connected!");
      // mqttClient.publish((espClientName + "_status").c_str(), "online", false, 0);
      mqttClient.subscribe((espClientName + "/command"), 0);
      mqttClient.subscribe((espClientName + "/rgb"), 0);
      mqttClient.subscribe("all/standby", 0);
      mqttClient.subscribe("all/command", 0);

      // mqttClient.publish((espClientName + "/status").c_str(), "online", false, 0);

      
    } else {
      delay(1000);
      Serial.println("MQTT Disconnected!");
    }
    mqttattempts++;
  }
  if (!mqttClient.connected() && mqttattempts >= 5) {
    Serial.println("\nMQTT connection failed...reboot");
    ESP.restart();
  }
}

void messageReceived(String& topic, String& payload) {
  Serial.println("Received message: " + topic + " - " + payload);

  if (topic == espClientName + "/rgb") {

    // Werte aus der Payload extrahieren
    int commaIndex1 = payload.indexOf(',');
    int commaIndex2 = payload.indexOf(',', commaIndex1 + 1);

    if (commaIndex1 != -1 && commaIndex2 != -1) {
      int red = payload.substring(0, commaIndex1).toInt();
      int green = payload.substring(commaIndex1 + 1, commaIndex2).toInt();
      int blue = payload.substring(commaIndex2 + 1).toInt();
      Serial.println(red);
      Serial.println(green);
      Serial.println(blue);

      setPWMColor(red, green, blue);
    } else {
      Serial.println("Fehler: Ungültige Payload");
    }
  } else if (topic == "all/standby") {
    if (payload == "on") {
      isStandby = true;
      saveLastColor();
      setPWMColor(0, 0, 0);
    } else if (payload == "off") {
      isStandby = false;
      restoreLastColor();
    }

  } else {
    Serial.println(currentColor);

    if (payload == "red") setColor(RED);
    else if (payload == "blue") setColor(BLUE);
    else if (payload == "green") setColor(GREEN);
    else if (payload == "yellow") setColor(YELLOW);
    else if (payload == "cyan") setColor(CYAN);
    else if (payload == "magenta") setColor(MAGENTA);
    else if (payload == "white") setColor(WHITE);
    else if (payload == "off") setColor(OFF);

 
  }
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
  }
  Serial.println("asd");
  mqttClient.publish((espClientName + "/status").c_str(), message.c_str(), true, 0);
  // mqttClient.publish((String(espClientName)).c_str(), message.c_str(), true, 2);
}


void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.reconnect();
  }
}


void handleIRReception() {
  if (IrReceiver.decode()) {
    Serial.print("IR Command: ");
    IrReceiver.printIRResultShort(&Serial);

    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      // Teamlead-Button gedrückt halten
      if (IrReceiver.decodedIRData.command == ir_tl_Command) {
        if (currentColor != GREEN) {
          setPWMColor(255, 0, 174);  // Aufflackern zur Visualisierung des Tastendrucks
          delay(100);
          setColor(GREEN);
          delay(1000);
        }

      } else if (IrReceiver.decodedIRData.command == irCommand) {
        // Normaler Befehl
        // if (!isTeamleadMode) {
        setPWMColor(255, 0, 174);  // Aufflackern zur Visualisierung des Tastendrucks
        delay(100);
        switchColor();  // Farbe wechseln
        delay(1000);
        // }
      }
    }

    IrReceiver.resume();
  }
}


void switchColor() {
  if (currentColor == GREEN) {
    setColor(WHITE);
  } else if (currentColor == RED) {
    setColor(YELLOW);
  } else if (currentColor == YELLOW) {
    setColor(RED);
  } else if (currentColor == WHITE) {
    setColor(RED);
  }
  sendCurrentState();
}

void setColor(Color color) {
  // if (currentColor != color) {
  fadeToColor(color);
  delay(100);
  saveLastColor();

  // }
}

void setPWMColor(int red, int green, int blue) {
  analogWrite(redLedPin, red);
  analogWrite(greenLedPin, green);
  analogWrite(BlueLedPin, blue);
}

void fadeToColor(Color targetColor) {
  const int fadeSpeed = 5;
  const int steps = 100;
  int redStart, greenStart, blueStart;
  int redEnd, greenEnd, blueEnd;

  getRGBValues(currentColor, redStart, greenStart, blueStart);
  getRGBValues(targetColor, redEnd, greenEnd, blueEnd);

  for (int i = 0; i <= steps; i++) {
    int redValue = redStart + ((redEnd - redStart) * i) / steps;
    int greenValue = greenStart + ((greenEnd - greenStart) * i) / steps;
    int blueValue = blueStart + ((blueEnd - blueStart) * i) / steps;

    setPWMColor(redValue, greenValue, blueValue);
    delay(fadeSpeed);
    // esp_task_wdt_reset(); // Watchdog während des Fading zurücksetzen
  }

  currentColor = targetColor;
}

void getRGBValues(Color color, int& red, int& green, int& blue) {
  switch (color) {
    case RED:
      red = 255;
      green = 0;
      blue = 0;
      break;
    case GREEN:
      red = 0;
      green = 255;
      blue = 0;
      break;
    case BLUE:
      red = 0;
      green = 0;
      blue = 255;
      break;
    case YELLOW:
      red = 255;
      green = 70;
      blue = 0;
      break;
    case CYAN:
      red = 0;
      green = 255;
      blue = 255;
      break;
    case MAGENTA:
      red = 255;
      green = 0;
      blue = 255;
      break;
    case WHITE:
      red = 255;
      green = 255;
      blue = 150;
      break;
    case OFF:
      red = 0;
      green = 0;
      blue = 0;
      break;
    default:
      red = 0;
      green = 0;
      blue = 0;
      break;
  }
}

void saveLastColor() {
  preferences.putUChar("lastColor", (uint8_t)currentColor);
  preferences.putBool("hasStoredColor", true);
}

void restoreLastColor() {
  if (preferences.getBool("hasStoredColor", false)) {
    Color savedColor = (Color)preferences.getUChar("lastColor", (uint8_t)CYAN);
    setColor(savedColor);
  }
}

void blinkPending() {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;

  if (millis() - lastBlinkTime >= 500) {
    lastBlinkTime = millis();
    ledState = !ledState;

    if (ledState) {
      setPWMColor(255, 0, 0);
    } else {
      setPWMColor(0, 0, 0);
    }
  }
}

