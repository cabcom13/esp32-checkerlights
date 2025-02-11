#include <IRremote.hpp>
#include <Preferences.h>
#include <array>
#include <BluetoothSerial.h>
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

// IR Commands
constexpr const char* tlcommand     = "7";
constexpr const char* nmcommand     = "9";

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
BluetoothSerial SerialBT;

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
    String ssid = preferences.getString("wifi_ssid", "");
  
    String password = preferences.getString("wifi_password", "");
    String tlcommand = preferences.getString("tlcommand", "7");
    String nmcommand = preferences.getString("nmcommand", "9");
    Serial.begin(115200);
    Serial.println("Booting...");
    Serial.println( tlcommand);  
    Serial.println( nmcommand);
    // Hardware initialisieren
    pinMode(RED_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);


    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);

    // IR-Empfänger stac:\Users\cabco\OneDrive\Dokumente\Arduino\libraries\ESP32-OTA\src\WebOTA.cpprten
    IrReceiver.begin(IR_RECV_PIN);

    if (ssid != "") {
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            flashColor(0xFF00AE);
            delay(1000);
            flashColor(0x000000);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Verbunden!");
            Serial.println(WiFi.localIP()); // IP-Adresse
            Serial.println(WiFi.RSSI());  // Signalstärke (dBm)
            int mqattempts = 0;
             mqttClient.setKeepAlive(120);
             while (!mqttClient.connected() && mqattempts < 10){ 
                mqttClient.begin(MQTT_SERVER.c_str(), wificlient);
                mqattempts++;
             }
             if(mqttClient.connected()){
                mqttClient.onMessage(mqttCallback);
                mqttClient.subscribe((String(MQTT_SERVER) + "/command"), 0);
                mqttconnected = true;
             } else {
                mqttconnected = false;
             }


            res = true;
        } else {
            res = false;
            SerialBT.begin(espname); // Bluetooth starten
            SerialBT.println("WLAN-Verbindung fehlgeschlagen. Starte Bluetooth...");
        }
    } else {
        res = false;
        SerialBT.begin(espname); // Bluetooth starten
        SerialBT.println("Kein gespeichertes WLAN gefunden. Warten auf Bluetooth-Befehl...");
    }
    // Letzte Farbe wiederherstellen
    setColor(currentColorIndex, true); 
}



void mqttCallback(String &topic, String &payload) {

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
      mqttClient.loop();
  }
  
if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    SerialBT.println("Empfangen: " + command);
   if (command.startsWith("espname:")) {
        String espname = command.substring(8);
        preferences.putString("espname", espname);
        SerialBT.println("Neuer ESPName gespeichert: " + espname);
    } else if (command.startsWith("wifi_ssid:")) {
        String ssid = command.substring(10);
        preferences.putString("wifi_ssid", ssid);
        SerialBT.println("SSID gespeichert: " + ssid);
    } else if (command.startsWith("wifi_password:")) {
        String password = command.substring(14);
        preferences.putString("wifi_password", password);
        SerialBT.println("Passwort gespeichert: ***");
    } else if (command.startsWith("tlcommand:")) {
        String tlcommand = command.substring(10);
        preferences.putString("tlcommand", tlcommand);
        SerialBT.println("TL Command saved");
    } else if (command.startsWith("nmcommand:")) {
        String tlcommand = command.substring(10);
        preferences.putString("nmcommand", nmcommand);
        SerialBT.println("NM Command saved");
    } else if (command ="restart") {
        ESP.restart();
    } else if (command.startsWith("wifi_start")) {
        // Stelle sicher, dass sowohl SSID als auch Passwort vorhanden sind
        String ssid = preferences.getString("wifi_ssid", "");
        String password = preferences.getString("wifi_password", "");

            SerialBT.println("Versuche Verbindung...");
            WiFi.begin(ssid.c_str(), password.c_str());
            while (WiFi.status() != WL_CONNECTED) {
                delay(1000);
                SerialBT.print(".");
            }
            SerialBT.println("\nVerbunden!");
            SerialBT.println(WiFi.localIP());         // IP-Adresse
            SerialBT.println(WiFi.RSSI());            // Signalstärke (dBm)

            res = true;
    }   
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
    uint32_t  tlC  = strtoul(tlcommand, nullptr, 10);
    uint32_t  nmC  = strtoul(nmcommand, nullptr, 10);
    if (!IrReceiver.decode()) return;
    const uint32_t command = IrReceiver.decodedIRData.command;
    if (command == tlC) {
       flashColor(0xFF00AE);
       setColor(2, true);
    }
    else if (command ==  nmC) {
        flashColor(0xFF00AE);
        cycleColors();
    }
    delay(500);
    IrReceiver.resume();
}

