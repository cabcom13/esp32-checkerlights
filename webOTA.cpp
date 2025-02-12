// Arduino build process info: https://github.com/arduino/Arduino/wiki/Build-Process

#define WEBOTA_VERSION "0.1.0"

#include "WebOTA.h"
#include <Arduino.h>
#include <WiFiClient.h>
// Füge diese Includes am Anfang der Datei hinzu
#include "esp_system.h"
#include "esp_chip_info.h"
#ifdef ESP32
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <Preferences.h>

WebServer OTAServer(9999);
#endif

#ifdef ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

ESP8266WebServer OTAServer(9999);
#endif

WebOTA webota;

extern Preferences preferences;

char WWW_USER[16]       = "";
char WWW_PASSWORD[16]   = "";
const char* WWW_REALM   = "WebOTA";
// the Content of the HTML response in case of Unautherized Access Default:empty
String authFailResponse = "Auth Fail";

////////////////////////////////////////////////////////////////////////////

int WebOTA::init(const unsigned int port, const char *path) {
	this->port = port;
	this->path = path;

	// Only run this once
	if (this->init_has_run) {
		return 0;
	}

	add_http_routes(&OTAServer, path);

#ifdef ESP32
	// https://github.com/espressif/arduino-esp32/issues/7708
	// Fix some slowness
	OTAServer.enableDelay(false);
#endif
	OTAServer.begin(port);

	// Serial.printf("WebOTA url   : http://%s.local:%d%s\r\n\r\n", this->mdns.c_str(), port, path);

	// Store that init has already run
	this->init_has_run = true;

	return 1;
}

// One param
int WebOTA::init(const unsigned int port) {
	return WebOTA::init(port, "/webota");
}

// No params
int WebOTA::init() {
	return WebOTA::init(80, "/webota");
}

void WebOTA::useAuth(const char* user, const char* password) {
	strncpy(WWW_USER, user, sizeof(WWW_USER) - 1);
	strncpy(WWW_PASSWORD, password, sizeof(WWW_PASSWORD) - 1);

	//Serial.printf("Set auth '%s' / '%s' %d\n", user, password, len);
}

int WebOTA::handle() {
	// If we haven't run the init yet run it
	if (!this->init_has_run) {
		WebOTA::init();
	}

	OTAServer.handleClient();
#ifdef ESP8266
	MDNS.update();
#endif

	return 1;
}

long WebOTA::max_sketch_size() {
	long ret = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

	return ret;
}

// R Macro string literal https://en.cppreference.com/w/cpp/language/string_literal
const char INDEX_HTML[] PROGMEM = R"!^!(
	<!doctype html>
	<html lang="en">
	<head>
		<meta charset="utf-8">
		<meta name="viewport" content="width=device-width, initial-scale=1">
		<title>Logix Light</title>
		<script src="main.js"></script>
		<link rel="stylesheet" href="main.css">
	</head>
	<body>
		<div class="container">
			<header class="header">
				<h1>Logix Light <span class="version">%s</span></h1>
			</header>
<div class="nav">
    <a href="/" class="nav-button">Dashboard</a>
    <a href="/settings" class="nav-button">Einstellungen</a>
    <a href="/webota" class="nav-button">Firmware</a>
</div>
			<main class="content">
				<div class="card upload-card">
					<form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
						<div class="file-input">
							<input type="file" name="update" id="file" accept=".bin">
							<label for="file">Choose firmware</label>
						</div>
						<button type="submit" class="btn primary">Upload Update</button>
					</form>
				</div>
	
				<div class="card info-card">
					<div class="info-item">
						<span class="label">Board:</span>
						<span class="value">%s</span>
					</div>
					<div class="info-item">
						<span class="label">MAC:</span>
						<span class="value">%s</span>
					</div>
					<div class="info-item">
						<span class="label">Uptime:</span>
						<span class="value">%s</span>
					</div>
				</div>
	
				<div id="prg" class="progress-bar">
					<div class="progress-fill"></div>
					<span class="progress-text">0%</span>
				</div>
			</main>
		</div>
	</body>
	</html>
	)!^!";
	
	const char MAIN_CSS[] PROGMEM = R"!^!(
		.header h1,body{margin:0;color:var(--text)}.btn.primary:hover,.file-input label:hover,.nav-button:hover{background:var(--primary-hover)}.btn,.label,.version{font-weight:500}.form-actions,.header,.upload-card{text-align:center}:root{--primary:#2196F3;--primary-hover:#1976D2;--background:#f5f5f5;--card-bg:#ffffff;--text:#333333;--border:#e0e0e0;--radius:8px;--shadow:0 2px 8px rgba(0,0,0,0.1)}.card,.file-input label{border-radius:var(--radius)}body{font-family:'Segoe UI',system-ui,sans-serif;background-color:var(--background);line-height:1.5}.container{max-width:800px;margin:0 auto;padding:1.5rem}.header{margin-bottom:2rem}.header h1{font-size:2.2rem}.version{color:var(--primary)}.card{background:var(--card-bg);box-shadow:var(--shadow);padding:1.5rem;margin-bottom:1.5rem}.btn,.file-input label{padding:.8rem 1.5rem;cursor:pointer}.file-input{position:relative;margin:1rem 0}.file-input input[type=file]{opacity:0;position:absolute;width:1px;height:1px}.file-input label{display:inline-block;background:var(--primary);color:#fff;transition:background .2s}.btn{border:none;border-radius:var(--radius);transition:.2s}.btn.primary{background:var(--primary);color:#fff;width:100%;max-width:200px}.btn.primary:hover{transform:translateY(-1px)}.info-card .info-item{display:flex;justify-content:space-between;padding:.8rem 0;border-bottom:1px solid var(--border)}.info-item:last-child{border-bottom:none}.label{color:#666}.value{color:var(--text)}.progress-bar{height:24px;background:#eee;border-radius:var(--radius);overflow:hidden;position:relative;display:none}.progress-fill{height:100%;background:var(--primary);width:0%;transition:width .3s}.progress-text{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);color:#fff;font-weight:700;text-shadow:0 1px 2px rgba(0,0,0,.2)}.form-group{margin-bottom:1.2rem}.form-input{width:100%;padding:.8rem;border:1px solid var(--border);border-radius:var(--radius);font-size:1rem;transition:border-color .2s}.form-input:focus{border-color:var(--primary);outline:0}.form-actions{margin-top:2rem}.settings-card{max-width:500px;margin:0 auto}@media (max-width:600px){.container{padding:1rem}.header h1{font-size:1.8rem}}.status-overview{display:flex;gap:1rem;margin-top:.5rem;font-size:.9rem}.status-item{background:rgba(0,0,0,.05);padding:.3rem .6rem;border-radius:1rem}.info-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem}.btn{margin:0 .5rem}.nav{margin:1rem 0 2rem;padding:0;display:flex;gap:.5rem}.nav-button{padding:.6rem 1.2rem;background:var(--primary);color:#fff;border:none;border-radius:var(--radius);text-decoration:none;font-size:.9rem}
	)!^!";
	
	const char MAIN_JS[] PROGMEM = R"!^!(
	document.addEventListener('DOMContentLoaded', () => {
		const form = document.getElementById('upload_form');
		const progressBar = document.getElementById('prg');
		const progressFill = progressBar.querySelector('.progress-fill');
		const progressText = progressBar.querySelector('.progress-text');
	
		form.onsubmit = async (e) => {
			e.preventDefault();
			
			const formData = new FormData();
			const file = document.getElementById('file').files[0];
			
			if (!file) return;
			
			formData.append('files', file);
			
			progressBar.style.display = 'block';
			
			const xhr = new XMLHttpRequest();
			
			xhr.upload.addEventListener('progress', (e) => {
				if (e.lengthComputable) {
					const percent = Math.round((e.loaded / e.total) * 100);
					progressFill.style.width = `${percent}%`;
					progressText.textContent = `${percent}%`;
				}
			});
			
			xhr.open('POST', window.location.href);
			
			xhr.onload = () => {
				setTimeout(() => {
					progressBar.style.display = 'none';
					progressFill.style.width = '0%';
					progressText.textContent = '0%';
				}, 2000);
			};
			
			xhr.send(formData);
		};
	});
	)!^!";

	const char SETTINGS_HTML[] PROGMEM = R"!^!(
		<!doctype html>
		<html lang="en">
		<head>
			<meta charset="utf-8">
			<meta name="viewport" content="width=device-width, initial-scale=1">
			<title>ESP Settings</title>
			<link rel="stylesheet" href="main.css">
		</head>
		<body>
			<div class="container">
				<header class="header">
					<h1>Device Settings</h1>
				</header>
<div class="nav">
    <a href="/" class="nav-button">Dashboard</a>
    <a href="/settings" class="nav-button">Einstellungen</a>
    <a href="/webota" class="nav-button">Firmware</a>
</div>
				<main class="content">
					<div class="card info-card">
						<div class="info-item">
							<span class="label">Device Name:</span>
							<span class="value">%s</span>
						</div>
						<div class="info-item">
							<span class="label">WiFi SSID:</span>
							<span class="value">%s</span>
						</div>
						<div class="info-item">
							<span class="label">MQTT Server:</span>
							<span class="value">%s</span>
						</div>
						<div class="info-item">
							<span class="label">TL Command:</span>
							<span class="value">%s</span>
						</div>
						<div class="info-item">
							<span class="label">NM Command:</span>
							<span class="value">%s</span>
						</div>
					</div>
		
					<div class="card action-card">
						<form method="POST" action="/restart" class="restart-form">
							<button type="submit" class="btn primary">Restart Device</button>
						</form>
					</div>
				</main>
			</div>
		</body>
		</html>
		)!^!";

		const char SETTINGS_FORM_HTML[] PROGMEM = R"!^!(
			<!doctype html>
			<html lang="en">
			<head>
				<meta charset="utf-8">
				<meta name="viewport" content="width=device-width, initial-scale=1">
				<title>Logix Light Settings</title>
				<link rel="stylesheet" href="main.css">
			</head>
			<body>
				<div class="container">
					<header class="header">
						<h1>Logix Light Settings</h1>
					</header>
<div class="nav">
    <a href="/" class="nav-button">Dashboard</a>
    <a href="/settings" class="nav-button">Einstellungen</a>
    <a href="/webota" class="nav-button">Firmware</a>
</div>
					<main class="content">
						<div class="card settings-card">
							<form method="POST" action="/sendir">
								<div class="form-group">
									<label for="wifi_ssid">SSID:</label>
									<input class="form-input" type="text" id="wifi_ssid" value="%s" name="wifi_ssid">
								</div>
			
								<div class="form-group">
									<label for="espClientName">ESP Client Name:</label>
									<input class="form-input" type="text" id="espClientName" value="%s" name="espClientName">
								</div>
			
								<div class="form-group">
									<label for="mqttserver">MQTT Server IP:</label>
									<input class="form-input" type="text" id="mqttserver" value="%s" name="mqttserver">
								</div>
			
								<div class="form-group">
									<label for="NMCOMMAND">TL COMMAND:</label>
									<input class="form-input" type="text" id="NMCOMMAND" value="%s" name="TLCOMMAND">
								</div>
			
								<div class="form-group">
									<label for="TLCOMMAND">NM COMMAND:</label>
									<input class="form-input" type="text" id="TLCOMMAND" value="%s" name="NMCOMMAND">
								</div>
			
								<div class="form-actions">
									<button type="submit" class="btn primary">Save Settings</button>
								</div>
							</form>
						</div>
					</main>
				</div>
			</body>
			</html>
			)!^!";
			const char DASHBOARD_HTML[] PROGMEM = R"!^!(
				<!doctype html>
				<html lang="en">
				<head>
					<meta charset="utf-8">
					<meta name="viewport" content="width=device-width, initial-scale=1">
					<title>ESP Dashboard</title>
					<link rel="stylesheet" href="main.css">
				</head>
				<body>
					<div class="container">
						<header class="header">
						
							<h1>%s</h1>
							<div class="status-overview">
								<span class="status-item">🟢 Online</span>
							
								<span class="status-item">💾 %d KB free</span>
							</div>
						</header>
<div class="nav">
    <a href="/" class="nav-button">Dashboard</a>
    <a href="/settings" class="nav-button">Einstellungen</a>
    <a href="/webota" class="nav-button">Firmware</a>
</div>
						<main class="content">
							<div class="card">
								<h2>Configuration</h2>
								<form method="POST" action="/sendir">
									<div class="form-group">
										<label>Device Name:</label>
										<input class="form-input" type="text" name="espname" value="%s">
									</div>
									<div class="form-group">
										<label>WiFi SSID:</label>
										<input class="form-input" type="text" name="wifi_ssid" value="%s">
									</div>
									<div class="form-group">
										<label>MQTT Server:</label>
										<input class="form-input" type="text" name="mqttserver" value="%s">
									</div>
									<div class="form-group">
										<label>TL Command:</label>
										<input class="form-input" type="text" name="TLCOMMAND" value="%s">
									</div>
									<div class="form-group">
										<label>NM Command:</label>
										<input class="form-input" type="text" name="NMCOMMAND" value="%s">
									</div>
									<div class="form-actions">
										<button type="submit" class="btn primary">Save Settings</button>
										<button type="button" class="btn" onclick="location.reload()">Refresh Status</button>
									</div>
								</form>
							</div>
				
							<div class="card">
								<h2>System Status</h2>
								<div class="info-grid">
									<div class="info-item">
										<span class="label">IP Address:</span>
										<span class="value">%s</span>
									</div>
									<div class="info-item">
										<span class="label">MAC Address:</span>
										<span class="value">%s</span>
									</div>
									<div class="info-item">
										<span class="label">WiFi Strength:</span>
										<span class="value">%d dBm</span>
									</div>
									<div class="info-item">
										<span class="label">Uptime:</span>
										<span class="value">%s</span>
									</div>
									<div class="info-item">
										<span class="label">Chip Model:</span>
										<span class="value">ESP32-%s</span>
									</div>
									<div class="info-item">
										<span class="label">SDK Version:</span>
										<span class="value">%s</span>
									</div>
								</div>
							</div>
						</main>
					</div>
				</body>
				</html>
				)!^!";
				
String WebOTA::get_board_type() {

// More information: https://github.com/search?q=repo%3Aarendst%2FTasmota%20esp32s2&type=code

#if defined(ESP8266)
	String BOARD_NAME = "ESP8266";
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
	String BOARD_NAME = "ESP32-S2";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
	String BOARD_NAME = "ESP32-S3";
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
	String BOARD_NAME = "ESP32-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32)
	String BOARD_NAME = "ESP32";
#elif defined(CONFIG_ARDUINO_VARIANT)
	String BOARD_NAME = CONFIG_ARDUINO_VARIANT;
#else
	String BOARD_NAME = "Unknown";
#endif

	return BOARD_NAME;
}
String formatUptime() {
		long ms = millis();
		return String(ms / 86400000) + "d " 
			+ String((ms % 86400000) / 3600000) + "h "
			+ String((ms % 3600000) / 60000) + "m";
	}

String get_mac_address() {
	uint8_t mac[6];

	// Put the addr in mac
	WiFi.macAddress(mac);

	// Build a string and return it
	char buf[20] = "";
	snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

	String ret = buf;

	return ret;
}

#ifdef ESP32
int8_t check_auth(WebServer *server) {
#endif
#ifdef ESP8266
int8_t check_auth(ESP8266WebServer *server) {
#endif
	// If we have a user and a password we check digest auth
	bool use_auth = (strlen(WWW_USER) && strlen(WWW_PASSWORD));
	if (!use_auth) {
		return 1;
	}

	if (!server->authenticate(WWW_USER, WWW_PASSWORD)) {
		//Basic Auth Method
		//return server.requestAuthentication(BASIC_AUTH, WWW_REALM, authFailResponse);

		// Digest Auth
		server->requestAuthentication(DIGEST_AUTH, WWW_REALM, authFailResponse);

		return 0;
	}

	return 2;
}

#ifdef ESP8266
int WebOTA::add_http_routes(ESP8266WebServer *server, const char *path) {
#endif
#ifdef ESP32
int WebOTA::add_http_routes(WebServer *server, const char *path) {
#endif
	// Index page
	server->on("/", HTTP_GET, [server]() {
		check_auth(server);
		
		preferences.begin("config", true);
		String espClientName = preferences.getString("espname", "ESP32-Device");
		String wifi_ssid = preferences.getString("wifi_ssid", "");
		String tlcommand = preferences.getString("tlcommand", "7");
		String nmcommand = preferences.getString("nmcommand", "9");
		String mqttserver = preferences.getString("mqttserver", "192.168.0.101");
		preferences.end();
	
		// Systemdaten mit Core-Funktionen
		float temp = temperatureRead(); // Von der Core-Bibliothek
		uint32_t free_mem = ESP.getFreeHeap() / 1024;
		String ip = WiFi.localIP().toString();
		String mac = WiFi.macAddress();
		int rssi = WiFi.RSSI();
		String uptime = formatUptime();
		
		esp_chip_info_t chip_info;
		esp_chip_info(&chip_info);
		const char* chip_model = "ESP32";
		
		char html[3000];
		snprintf_P(html, sizeof(html), DASHBOARD_HTML,
				   espClientName.c_str(),
		
				   free_mem,
				   espClientName.c_str(),
				   wifi_ssid.c_str(),
				   mqttserver.c_str(),
				   tlcommand.c_str(),
				   nmcommand.c_str(),
				   ip.c_str(),
				   mac.c_str(),
				   rssi,
				   uptime.c_str(),
				   chip_model,
				   ESP.getSdkVersion());
	
		server->send(200, "text/html", html);
	});

    // New page for espClientName, host, IRCommand
    server->on("/settings", HTTP_GET, [server,this]() {
        check_auth(server);


		// Initialisierung und Speichern der Werte
        preferences.begin("config", true);
		String espClientName = preferences.getString("espname", "default");
		String mqttserver = preferences.getString("mqttserver", "192.168.0.101");
		
		String wifi_ssid = preferences.getString("wifi_ssid", "default");
		String tlcommand = preferences.getString("tlcommand", "7");
		String nmcommand = preferences.getString("nmcommand", "9");
        

        preferences.end();
		String ir_html;
		char htmlBuffer[2000]; // Puffer je nach benötigter Größe anpassen
		snprintf_P(htmlBuffer, sizeof(htmlBuffer), SETTINGS_FORM_HTML,
				   wifi_ssid.c_str(),
				   espClientName.c_str(),
				   mqttserver.c_str(),
				   tlcommand.c_str(),
				   nmcommand.c_str());
		
		server->send(200, "text/html", htmlBuffer);
    });

    // Handling POST request for sending IR command
    server->on("/restart", HTTP_POST, [server,this]() {
        check_auth(server);
        server->send(200, "text/plain", "Restart läuft");
		delay(1000);
		ESP.restart();
    });

    // Handling POST request for sending IR command

    server->on("/sendir", HTTP_POST, [server,this]() {
        check_auth(server);

        String wifi_ssid = server->arg("wifi_ssid");
		String espClientName = server->arg("espClientName");
        String mqttserver = server->arg("mqttserver");
        String TLCommand = server->arg("TLCOMMAND");
		String NMCommand = server->arg("NMCOMMAND");

		// Konvertiere den String in einen int
		// int TLCommandInt = strtol(TLCommand.c_str(), NULL, 16);
		// int NMCommandInt = strtol(NMCommand.c_str(), NULL, 16);


		// Initialisierung und Speichern der Werte
		preferences.begin("config", false);
		preferences.putString("espname", espClientName);
		preferences.putString("mqttserver", mqttserver);
		preferences.putString("wifi_ssid", wifi_ssid);
		preferences.putString("tlcommand", TLCommand);
		preferences.putString("nmcommand", NMCommand);
		preferences.end();

        server->send(200, "text/plain", "Settings saved");
		delay(1000);
		ESP.restart();
    });


	// Upload firmware page
	server->on(path, HTTP_GET, [server,this]() {
		check_auth(server);

		String html = "";
		if (this->custom_html != NULL) {
			html = this->custom_html;
		} else {
			//uint32_t maxSketchSpace = this->max_sketch_size();

			String uptime_str = human_time(millis() / 1000);
			String board_type = webota.get_board_type();
			String mac_addr   = get_mac_address();

			char buf[1024];
			snprintf_P(buf, sizeof(buf), INDEX_HTML, WEBOTA_VERSION, board_type, mac_addr.c_str(), uptime_str.c_str());

			html = buf;
		}

		server->send_P(200, "text/html", html.c_str());
	});

	// Handling uploading firmware file
	server->on(path, HTTP_POST, [server,this]() {
		check_auth(server);

		server->send(200, "text/plain", (Update.hasError()) ? "Update: fail\n" : "Update: OK!\n");
		delay(500);
		ESP.restart();
	}, [server,this]() {
		HTTPUpload& upload = server->upload();

		if (upload.status == UPLOAD_FILE_START) {
			Serial.printf("Firmware update initiated: %s\r\n", upload.filename.c_str());

			//uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
			uint32_t maxSketchSpace = this->max_sketch_size();

			if (!Update.begin(maxSketchSpace)) { //start with max available size
				Update.printError(Serial);
			}
		} else if (upload.status == UPLOAD_FILE_WRITE) {
			/* flashing firmware to ESP*/
			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
				Update.printError(Serial);
			}

			// Store the next milestone to output
			uint16_t chunk_size  = 51200;
			static uint32_t next = 51200;

			// Check if we need to output a milestone (100k 200k 300k)
			if (upload.totalSize >= next) {
				Serial.printf("%dk ", next / 1024);
				next += chunk_size;
			}
		} else if (upload.status == UPLOAD_FILE_END) {
			if (Update.end(true)) { //true to set the size to the current progress
				Serial.printf("\r\nFirmware update successful: %u bytes\r\nRebooting...\r\n", upload.totalSize);
			} else {
				Update.printError(Serial);
			}
		}
	});

	// FILE: main.js
	server->on("/main.js", HTTP_GET, [server]() {
		server->send_P(200, "application/javascript", MAIN_JS);
	});

	// FILE: main.css
	server->on("/main.css", HTTP_GET, [server]() {
		server->send_P(200, "text/css", MAIN_CSS);
	});

	// FILE: favicon.ico
	server->on("/favicon.ico", HTTP_GET, [server]() {
		server->send(200, "image/vnd.microsoft.icon", "");
	});

	server->begin();

	return 1;
}

// If the MCU is in a delay() it cannot respond to HTTP OTA requests
// We do a "fake" looping delay and listen for incoming HTTP requests while waiting
void WebOTA::delay(unsigned int ms) {
	// Borrowed from mshoe007 @ https://github.com/scottchiefbaker/ESP-WebOTA/issues/8
	decltype(millis()) last = millis();

	while ((millis() - last) < ms) {
		OTAServer.handleClient();
		::delay(5);
	}
}

void WebOTA::set_custom_html(char const * const html) {
	this->custom_html = html;
}
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

int init_mdns(const char *host) {
	// Use mdns for host name resolution
	if (!MDNS.begin(host)) {
		Serial.println("Error setting up MDNS responder!");

		return 0;
	}

	Serial.printf("mDNS started : %s.local\r\n", host);

	webota.mdns = host;

	return 1;
}

String ip2string(IPAddress ip) {
	String ret = String(ip[0]) + "." +  String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

	return ret;
}

String WebOTA::human_time(uint32_t sec) {
    int days = (sec / 86400);
    sec = sec % 86400;
    int hours = (sec / 3600);
    sec = sec % 3600;
    int mins  = (sec / 60);
    sec = sec % 60;

	char buf[24] = "";
	if (days) {
        snprintf(buf, sizeof(buf), "%d days %d hours\n", days, hours);
    } else if (hours) {
        snprintf(buf, sizeof(buf), "%d hours %d minutes\n", hours, mins);
    } else {
        snprintf(buf, sizeof(buf), "%d minutes %d seconds\n", mins, sec);
    }

	String ret = buf;

	return ret;
}

int init_wifi(const char *ssid, const char *password, const char *mdns_hostname) {
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	Serial.println("");
	Serial.print("Connecting to Wifi");

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.printf("Connected to '%s'\r\n\r\n",ssid);

	String ipaddr = ip2string(WiFi.localIP());
	Serial.printf("IP address   : %s\r\n", ipaddr.c_str());
	Serial.printf("MAC address  : %s \r\n", WiFi.macAddress().c_str());

	init_mdns(mdns_hostname);

	return 1;
}
