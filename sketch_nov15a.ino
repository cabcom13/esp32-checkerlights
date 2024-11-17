#define DECODE_NEC

#include <WiFi.h>
// #include <PubSubClient.h>
#include <MQTTClient.h>
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

// const char* espClientName = "w1a";
// const char* host = "ESP-w1a";
// const int irCommand = 0x9;

// Globale Variablen
String espClientName;
String host;
int irCommand;

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
    RED, YELLOW, GREEN, BLUE, CYAN, MAGENTA, WHITE, OFF
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

    preferences.begin("led-state", true);
    espClientName = preferences.getString("espClientName", "default");
    host = preferences.getString("host", "default");
    irCommand = preferences.getInt("IRCOMMAND");
    preferences.end();

    Serial.println("espClientName: " + espClientName);
    Serial.println("Host: " + host);
    Serial.printf("IR Command: 0x%X\n", irCommand);

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

// void setupMQTT() {
//     client.setServer(mqttServer, mqttPort);
//     client.setCallback(mqttCallback);
//     client.setKeepAlive(60);
// }

void setupMQTT() {
    mqttClient.begin(mqttServer, espClient);
    mqttClient.onMessage(messageReceived);
    connectMQTT();
}

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT... ");
        const char* lwtTopic = (String(espClientName) + "_status").c_str();
        const char* lwtMessage = "offline";

        // LWT konfigurieren
        mqttClient.setWill(lwtTopic, lwtMessage, true, 2);

        // MQTT-Verbindung herstellen
        if (mqttClient.connect(espClientName.c_str(), "user", "password")) {
            Serial.println("connected!");
            mqttClient.subscribe(espClientName.c_str(), 2);
            mqttClient.subscribe("all", 2);
            mqttClient.publish((String(espClientName) + "_status").c_str(), "online", false, 2);
        } else {
            Serial.println("failed, retrying...");
            delay(MQTT_RETRY_DELAY);
        }
    }
}

void messageReceived(String &topic, String &payload) {
    Serial.println("Received message: " + topic + " - " + payload);

    if (topic == "all" && payload == "who_is_here") {
        sendCurrentState();
    }

    if (topic == espClientName || topic == "all") {
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

void loop() {


    // WiFi-Verbindung prüfen
    if (millis() - lastWiFiCheck >= wifiCheckInterval) {
        checkWiFiConnection();
        lastWiFiCheck = millis();
    }

    // MQTT-Verbindung prüfen
    if (!mqttClient.connected()) {
        connectMQTT();
    }

    
    // // Status-Update senden
    // if (millis() - lastSendTime >= sendInterval) {
    //     lastSendTime = millis();
    //     sendCurrentState();
    //     if (client.connected()) {
    //       client.publish((String(espClientName) + "_data").c_str(), "connected");
    //     }
    // }

    mqttClient.loop();

    // IR-Empfang prüfen
    handleIRReception();

    // OTA Updates behandeln
    webota.handle();


        // Watchdog Reset
    esp_task_wdt_reset();
delay(10);  // Kurze Pause, um den Watchdog zu füttern
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        WiFi.reconnect();
    }
}

void reconnectMQTT() {
    int retries = 0;
    
    while (!mqttClient.connected() && retries < MQTT_MAX_RETRIES) {
        String clientId = "ESP_" + String(espClientName);
        const char* lwtTopic = (String(espClientName) + "_status").c_str();
        const char* lwtMessage = "offline";
        // if (client.connect(clientId.c_str(), NULL, NULL, lwtTopic, 0, true, lwtMessage, true)) {
        if (mqttClient.connect(clientId.c_str())) {
            mqttClient.subscribe(espClientName.c_str(), 2);
            mqttClient.subscribe("all", 2);
            mqttClient.publish((String(espClientName) + "_status").c_str(), "online", false, 2);
            Serial.println("MQTT Connected");
            break;
        }
        
        retries++;
        Serial.println("MQTT connection failed, retry " + String(retries));
        delay(MQTT_RETRY_DELAY);
        esp_task_wdt_reset(); // Watchdog während der Verbindungsversuche zurücksetzen
    }
    
    if (!mqttClient.connected()) {
        ESP.restart();
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = String((char*)payload).substring(0, length);
    if (String(topic) == espClientName || String(topic) == "all") {
        Serial.println(topic);
        Serial.println("MQTT Topic: "+ message);

        // Überprüfen, ob eine "who_is_here"-Anfrage empfangen wurde
        if (String(topic) == "all") {
            if(message == "who_is_here"){
            sendCurrentState();
            }
            
        }

        if (message == "red") setColor(RED);
        else if (message == "blue") setColor(BLUE);
        else if (message == "green") setColor(GREEN);
        else if (message == "yellow") setColor(YELLOW);
        else if (message == "cyan") setColor(CYAN);
        else if (message == "magenta") setColor(MAGENTA);
        else if (message == "white") setColor(WHITE);
        else if (message == "off") setColor(OFF);

        // sendCurrentState();
    }
}

void handleIRReception() {
    if (IrReceiver.decode()) {
        if (IrReceiver.decodedIRData.protocol != UNKNOWN &&
            IrReceiver.decodedIRData.command == irCommand) {
            if (currentColor != OFF && 
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
    }
    mqttClient.publish((String(espClientName)).c_str(), message.c_str(), true, 2);
}
// void sendCurrentState() {
//     String message;
//     switch (currentColor) {
//         case RED: message = "red"; break;
//         case YELLOW: message = "yellow"; break;
//         case GREEN: message = "green"; break;
//         case BLUE: message = "blue"; break;
//         case CYAN: message = "cyan"; break;
//         case MAGENTA: message = "magenta"; break;
//         case WHITE: message = "white"; break;
//         case OFF: message = "off"; break;

//     }
//     client.publish(espClientName.c_str(), message.c_str());
// }