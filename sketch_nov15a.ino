#define DECODE_NEC

#include <WiFi.h>
#include <PubSubClient.h>
#include <IRremote.hpp>
#include <WebOTA.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// Watchdog Timeout in Sekunden
#define WDT_TIMEOUT 30

// WiFi und MQTT Konfiguration
const char* ssid = "TP-LINK_B383";
const char* password = "cda619872";
const char* mqttServer = "192.168.0.101";
const int mqttPort = 1883;
const int MQTT_RETRY_DELAY = 5000;
const int MQTT_MAX_RETRIES = 3;

const char* espClientName = "w1a";
const char* host = "ESP-w1a";
const int IRCOMMAND = 0x9;

// Hardware-Pins
const int redLedPin = 15;
const int BlueLedPin = 5;
const int greenLedPin = 4;
const int RECV_PIN = 18;

// Objekte
WiFiClient espClient;
PubSubClient client(espClient);
IRrecv irrecv(RECV_PIN);
extern Preferences preferences;

// Status-Variablen
enum Color {
    RED, YELLOW, GREEN, BLUE, CYAN, MAGENTA, WHITE, OFF, PENDING
};
Color currentColor = CYAN;
bool buttonPressed = false;
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

      // Lesen der Werte
      preferences.begin("my-app", true);
      String espClientName = preferences.getString("espClientName", "default");
      String host = preferences.getString("host", "default");
      String irCommand = preferences.getString("IRCOMMAND", "default");
      preferences.end();

      Serial.println(espClientName);
      Serial.println(host);
      Serial.println(irCommand);
    // Watchdog Timer initialisieren
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,  // Timeout in Millisekunden
        .idle_core_mask = 0,              // Watchdog wird auf allen Kernen überwacht
        .trigger_panic = true             // Gerät wird bei Timeout neugestartet
    };
    esp_task_wdt_init(&wdt_config);

    // Task zur Überwachung hinzufügen
    esp_task_wdt_add(NULL);

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
    restoreLastColor();


}

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        setPWMColor(255, 0, 174);
        delay(500);
        setPWMColor(0, 0, 0);
        delay(500);
        attempts++;
        esp_task_wdt_reset(); // Watchdog während der Verbindungsversuche zurücksetzen
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.println("IP address: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi connection failed");
        ESP.restart();
    }
}

void setupMQTT() {
    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback);
    client.setKeepAlive(60);
}

void loop() {
    // Watchdog Reset
    esp_task_wdt_reset();


    // WiFi-Verbindung prüfen
    if (millis() - lastWiFiCheck >= wifiCheckInterval) {
        checkWiFiConnection();
        lastWiFiCheck = millis();
    }

    // MQTT-Verbindung prüfen
    if (!client.connected()) {
        reconnectMQTT();
    }
    

    // Pending-Status behandeln
    if (currentColor == PENDING) {
        blinkPending();
    }

    // Status-Update senden
    if (millis() - lastSendTime >= sendInterval) {
        lastSendTime = millis();
        sendCurrentState();
    }

    client.loop();

    // IR-Empfang prüfen
    handleIRReception();

    // OTA Updates behandeln
    webota.handle();
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        WiFi.reconnect();
    }
}

void reconnectMQTT() {
    int retries = 0;
    
    while (!client.connected() && retries < MQTT_MAX_RETRIES) {
        String clientId = "ESP_" + String(espClientName);
        
        if (client.connect(clientId.c_str())) {
            client.subscribe(espClientName);
            client.subscribe("all");
            client.publish((String(espClientName) + "_data").c_str(), "connected");
            Serial.println("MQTT Connected");
            break;
        }
        
        retries++;
        Serial.println("MQTT connection failed, retry " + String(retries));
        delay(MQTT_RETRY_DELAY);
        esp_task_wdt_reset(); // Watchdog während der Verbindungsversuche zurücksetzen
    }
    
    if (!client.connected()) {
        ESP.restart();
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = String((char*)payload).substring(0, length);
    if (String(topic) == espClientName || String(topic) == "all") {
        Serial.println(topic);
        Serial.println("MQTT Topic: "+ message);
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
            if (currentColor != OFF && currentColor != PENDING && 
                currentColor != CYAN && currentColor != MAGENTA && 
                currentColor != BLUE) {
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
    if (currentColor == GREEN) {
        setColor(WHITE);
    }
    else if (currentColor == RED) {
        setColor(YELLOW);
    }
    else if (currentColor == YELLOW) {
        setColor(RED);
    }
    else if (currentColor == WHITE) {
        setColor(RED);
    }
    sendCurrentState();
}

void setColor(Color color) {
    // if (currentColor != color) {
        fadeToColor(color);
        saveLastColor();
    // }
}

void setPWMColor(int red, int green, int blue) {
    analogWrite(redLedPin, red);
    analogWrite(greenLedPin, green);
    analogWrite(BlueLedPin, blue);
}

void fadeToColor(Color targetColor) {
    const int fadeSpeed = 10;
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
        esp_task_wdt_reset(); // Watchdog während des Fading zurücksetzen
    }
    
    currentColor = targetColor;

}

void getRGBValues(Color color, int &red, int &green, int &blue) {
    switch (color) {
        case RED: red = 255; green = 0; blue = 0; break;
        case GREEN: red = 0; green = 255; blue = 0; break;
        case BLUE: red = 0; green = 0; blue = 255; break;
        case YELLOW: red = 255; green = 60; blue = 0; break;
        case CYAN: red = 0; green = 255; blue = 255; break;
        case MAGENTA: red = 255; green = 0; blue = 255; break;
        case WHITE: red = 255; green = 255; blue = 160; break;
        case OFF: red = 0; green = 0; blue = 0; break;
        default: red = 0; green = 0; blue = 0; break;
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