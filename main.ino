#include <IRremote.hpp>
#include <Preferences.h>
#include <array>
// #include <BluetoothSerial.h>
#include <WiFi.h>
#include <MQTTClient.h>
#include <WebOTA.h>

// Farbdefinitionen
struct Color {
    const char* name;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr std::array<Color, 8> COLORS = {{
    {"red",     255, 0,   0},
    {"yellow",  255, 70,  0},
    {"green",   0,   255, 0},
    {"blue",    0,   0,   255},
    {"cyan",    0,   255, 255},
    {"magenta", 255, 0,   255},
    {"white",   255, 255, 150},
    {"off",     0,   0,   0}
}};
// Definiere eine Task-Handle für MQTT
TaskHandle_t mqttTaskHandle = NULL;
// IR Commands
String tlcommand     = "7";
String nmcommand     = "9";

// Hardware-Pins
constexpr uint8_t RED_PIN     = 15;
constexpr uint8_t BLUE_PIN    = 5;
constexpr uint8_t GREEN_PIN   = 4;
constexpr uint8_t IR_RECV_PIN = 18;

// Konfiguration
String SSID;
String PASSWORD;
String MQTT_SERVER;
String espname;
bool res = false;
bool mqttconnected = false;
const int mqttPort = 1883;

// Globale Objekte
IRrecv irrecv(IR_RECV_PIN);
Preferences preferences;
WiFiClient wificlient;
MQTTClient mqttClient(256);
// BluetoothSerial SerialBT;

bool otaEnabled = false;
// Zustandsvariablen
size_t currentColorIndex = 6; // Start mit Weiß

void setup() {

    //Preferences initialisieren und prüfen, ob Konfiguration vorliegt
    preferences.begin("config", false);
    currentColorIndex = preferences.getUChar("lastColor", 6);

    MQTT_SERVER = preferences.getString("mqttserver", "192.168.0.101");

    // Wenn SSID, Passwort und ESPName in Preferences gespeichert sind, verwende sie
    espname = preferences.getString("espname", "ESP32_Default");
 
    if (espname.isEmpty()) {
      espname = "ESP32_" + String(random(1000, 9999));
      preferences.putString("espname", espname);
    }

    String ssid = preferences.getString("wifi_ssid", "");
    String password = preferences.getString("wifi_password", "");
    tlcommand = preferences.getString("tlcommand", "7");
    nmcommand = preferences.getString("nmcommand", "9");

    Serial.begin(115200);
    // serial.println("Booting...");

    // Hardware initialisieren
    pinMode(RED_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);


    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);

    // Starte MQTT-Task
    xTaskCreatePinnedToCore(
      mqttLoopTask,          // Task Funktion
      "MQTT Loop Task",      // Task Name
      4096,                  // Stack Größe
      NULL,                  // Parameter
      1,                     // Priorität
      &mqttTaskHandle,       // Task Handle
      1                      // Core (1 ist der zweite Core des ESP32)
    );

    // IR-Empfänger stac:\Users\cabco\OneDrive\Dokumente\Arduino\libraries\ESP32-OTA\src\WebOTA.cpprten
    IrReceiver.begin(IR_RECV_PIN);

    if (ssid != "") {
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            flashColor(0xFF00AE);
            delay(1000);
            flashColor(0x000000);
            // serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            // serial.println("Verbunden!");
            // serial.println(WiFi.localIP()); // IP-Adresse
            // serial.println(WiFi.RSSI());  // Signalstärke (dBm)
            int mqattempts = 0;
             mqttClient.begin(MQTT_SERVER.c_str(), wificlient);
             mqttClient.setKeepAlive(120);
             mqttClient.onMessage(mqttCallback);
           
             // serial.println("Verbinde MQTT");
             while (!mqttClient.connected() && mqattempts < 10){
                // serial.print(" .");
                mqttClient.connect(espname.c_str()); 
                mqattempts++;
                if(mqttClient.connected()){   
                    mqttClient.subscribe((String(espname) + "/command"), 0);         
                    mqttClient.publish((espname + "/status").c_str(), "online", false, 0);
                    sendCurrentState();
                    mqattempts = 0;
                    mqttconnected = true;
                } else {
                    // serial.println(mqttClient.lastError());
                    mqttconnected = false;
                }
                delay(1000);
             }

            res = true;
        } else {
            res = false;
            // WiFi.softAP(espname.c_str());
            // otaEnabled = true;
            // serial.print("AP gestartet: ");
            // serial.println(espname);
            // serial.print("IP Adresse: ");
            // serial.println(WiFi.softAPIP());
        }
    } else {
        res = false;
        WiFi.softAP(espname.c_str());
        otaEnabled = true;
        // serial.print("AP gestartet: ");
        // serial.println(espname);
        // serial.print("IP Adresse: ");
        // serial.println(WiFi.softAPIP());
    }
    // Letzte Farbe wiederherstellen
    setColor(currentColorIndex, true); 
}

void sendCurrentState() {
  mqttClient.publish((espname + "/status").c_str(), COLORS[currentColorIndex].name, false, 0);
}

void mqttCallback(String &topic, String &payload) {
// red:0
// yellow: 1
// green: 2
// blue: 3
// cyan: 4
// magenta: 5
// white: 6
// off: 7
  // serial.println(payload);
    if (payload == "red") setColor(0, true);   
    else if (payload == "yellow") setColor(1, true); 
    else if (payload == "green") setColor(2, true); 
    else if (payload == "blue") setColor(3, true); 
    else if (payload == "cyan") setColor(4, true); 
    else if (payload == "magenta") setColor(5, true); 
    else if (payload == "white") setColor(6, true); 
    else if (payload == "off") setColor(7, true); 
    else if (payload == "wiyc") sendCurrentState();
    else if (payload == "standbyon") flashColor(0x000000);
    else if (payload == "standbyoff") setColor(currentColorIndex, true); 
}

void loop() {

  if(res == false){
    flashColor(0x004cff);
    delay(100);
    flashColor(0x000000);
    delay(100);
  }
  if(mqttconnected == false && res == true){
    flashColor(0x00d5ff);
    delay(2000);
    flashColor(0x000000);
    delay(2000);
  }
  

  if(res && mqttconnected){
    handleIRReception();
  }
  // OTA Updates behandeln
  if(WiFi.status() == WL_CONNECTED){
      webota.handle();
      // mqttClient.loop();
  }

  if(otaEnabled){
    webota.handle();
  }
  delay(10);
}
// MQTT-Loop als FreeRTOS-Task
void mqttLoopTask(void *parameter) {
  while (true) {
    if (mqttconnected) {
      mqttClient.loop();
    }
    delay(10); // Wartezeit, um CPU zu entlasten
  }
}
void cycleColors() {
  
    switch (currentColorIndex) {
        case 6: // Aktuell Weiß
            setColor(0, true); 
            break;
        case 0: // Aktuell Rot
            setColor(1, true); 
            break;
        case 1: // Aktuell Orange
            setColor(0, true); 
            break;
        case 2:
            setColor(6, true); 
            break;
        default:
            break;
    }
    sendCurrentState();
    delay(500);
}

void setColor(size_t index, bool save) {
    if (index >= COLORS.size()) return;
    currentColorIndex = index;
    const Color &c = COLORS[index];
    analogWrite(RED_PIN, c.red);
    analogWrite(GREEN_PIN, c.green);
    analogWrite(BLUE_PIN, c.blue);
    if (save) {
        preferences.putUChar("lastColor", index);
    }
}
void setColorDirect(uint32_t color) {
    analogWrite(RED_PIN,   (color >> 16) & 0xFF);
    analogWrite(GREEN_PIN, (color >> 8)  & 0xFF);
    analogWrite(BLUE_PIN,  color & 0xFF);
}
void flashColor(uint32_t color) {
    setColorDirect(color);
    delay(150);

}

// ---------- IR-Handling ----------
void handleIRReception() {
    uint32_t tlC = strtoul(tlcommand.c_str(), nullptr, 10);
    uint32_t  nmC  = strtoul(nmcommand.c_str(), nullptr, 10);
    if (!IrReceiver.decode()) return;
    const uint32_t command = IrReceiver.decodedIRData.command;
    if(command == 80){
          WiFi.softAP(espname.c_str());
          otaEnabled = true;
          res = false;
    }
    if (command == tlC) {
       flashColor(0xFF00AE);
       setColor(2, true);
       sendCurrentState();
    }
    else if (command ==  nmC) {
        flashColor(0xFF00AE);
        cycleColors();
    }
    delay(500);
    IrReceiver.resume();
}

