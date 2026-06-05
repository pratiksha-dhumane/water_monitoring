// ============================================================
//  Water Quality Monitor — ESP32 Gateway Node
//  • Receives data from Arduino (NRF24L01+)
//  • Parses T / PH / TURB values
//  • Displays on 16×2 I²C LCD (address 0x27)
//  • POSTs JSON to backend over WiFi
//  • Logs to SPIFFS (/data.txt) as local backup
//
//  Required libraries (install via Arduino Library Manager):
//    Adafruit_SSD1306  — Adafruit (for OLED display)
//    ArduinoJson         — Benoit Blanchon
//    RF24                — TMRh20
//    (WiFi + HTTPClient are built into ESP32 Arduino core)
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <RF24.h>
#include "SPIFFS.h"

// ── OLED Display (128x64 I²C SSD1306) ──────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // Default I2C address for SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── WiFi credentials ─────────────────────────────────────────
const char* WIFI_SSID     = "rupesh009";
const char* WIFI_PASSWORD = "rupesh009";

// ── Backend API ───────────────────────────────────────────────
const char* SERVER_URL = "http://10.231.49.14:3000/sensor-data";
// Example: "http://192.168.1.100:3000/sensor-data"

// ── NRF24L01+ wireless link ──────────────────────────────────
#define NRF_CE_PIN     12    // ESP32 GPIO12 for CE (Chip Enable)
#define NRF_CSN_PIN    14    // ESP32 GPIO14 for CSN (Chip Select Not)
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte addr[6] = "00001";  // 5-byte address for pipe 0
#define NRF_PAYLOAD_SIZE 32
char nrf_payload[NRF_PAYLOAD_SIZE];

// ── Timing ───────────────────────────────────────────────────
#define POST_INTERVAL_MS   10000   // send to backend every 10 s
unsigned long lastPost = 0;

// ── Sensor value globals ─────────────────────────────────────
float gTemperature = 0.0;
float gPH          = 0.0;
float gTurbidity   = 0.0;
bool  gDataReady   = false;

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);             // debug monitor
  delay(1000);  // Give serial monitor time to connect
  Serial.println("\n\n[STARTUP] Water Monitor initializing...");
  
  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init failed!");
    while (1) { delay(1000); }  // Halt
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Water Monitor");
  display.println("NRF Init...");
  display.display();

  // Initialize NRF24L01+
  SPI.begin(18, 19, 23);  // ESP32 SPI pins: CLK=18, MISO=19, MOSI=23
  if (!radio.begin()) {
    Serial.println("[NRF] Init failed!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("NRF Init Failed!");
    display.display();
    while (1) { delay(1000); }  // Halt
  }
  radio.setPALevel(RF24_PA_LOW);      // Low power for close range
  radio.setDataRate(RF24_250KBPS);    // Slow but reliable
  radio.openReadingPipe(1, addr);
  radio.startListening();             // ESP32 is RX only
  Serial.println("[NRF] Initialized OK");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Water Monitor");
  display.println("NRF Ready...");
  display.display();

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Mount failed");
  } else {
    Serial.println("[SPIFFS] Mounted OK");
    ensureCSVHeader();
  }

  // WiFi
  connectWiFi();
  
  // If WiFi fails on startup, retry once after delay
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] First attempt failed, retrying...");
    delay(3000);
    connectWiFi();
  }
}

void loop() {
  // 1. Try reading from NRF24L01
  if (radio.available()) {
    memset(nrf_payload, 0, NRF_PAYLOAD_SIZE);
    radio.read(&nrf_payload, NRF_PAYLOAD_SIZE);
    String raw = String(nrf_payload);
    raw.trim();
    if (raw.length() > 0) {
      Serial.println("[RX-NRF] " + raw);
      if (parseData(raw)) {
        showOnDisplay();
        logToSPIFFS();
        gDataReady = true;
      }
    }
  }

  // 2. Check WiFi connection periodically
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {  // Check every 30 seconds
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Disconnected — reconnecting...");
      connectWiFi();
    }
  }

  // 3. POST to backend on interval
  if (gDataReady && (millis() - lastPost >= POST_INTERVAL_MS)) {
    if (WiFi.status() == WL_CONNECTED) {
      postToBackend();
    } else {
      Serial.println("[WiFi] Offline — skipping POST (data will be in SPIFFS log)");
    }
    lastPost = millis();
  }
}

// ─── Parse "T:25.5,PH:7.2,TURB:300" ─────────────────────────
bool parseData(const String& raw) {
  // Expect exactly: T:<float>,PH:<float>,TURB:<int>
  int tIdx    = raw.indexOf("T:");
  int phIdx   = raw.indexOf(",PH:");
  int turbIdx = raw.indexOf(",TURB:");

  if (tIdx < 0 || phIdx < 0 || turbIdx < 0) {
    Serial.println("[Parse] Bad format: " + raw);
    return false;
  }

  gTemperature = raw.substring(tIdx + 2, phIdx).toFloat();
  gPH          = raw.substring(phIdx + 4, turbIdx).toFloat();
  gTurbidity   = raw.substring(turbIdx + 6).toFloat();

  Serial.printf("[Parse] Temp=%.1f  pH=%.1f  Turb=%.0f\n",
                gTemperature, gPH, gTurbidity);
  return true;
}

// ─── OLED display ────────────────────────────────────────────
void showOnDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // Line 0: Title
  display.println("=== Water Quality ===");

  // Line 1: Temperature
  display.print("Temp: ");
  display.print(gTemperature, 1);
  display.println("C");

  // Line 2: pH
  display.print("pH: ");
  display.println(gPH, 1);

  // Line 3: Turbidity
  display.print("Turb: ");
  display.print((int)gTurbidity);
  display.println(" NTU");

  // Line 4: Status
  display.print("Status: ");
  display.println(waterStatus());

  // Line 5: Separator
  display.println("-----");

  // Line 6-7: Additional info
  display.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println("OK");
  } else {
    display.println("Offline");
  }

  display.display();
}

// ─── Water status string ─────────────────────────────────────
String waterStatus() {
  bool tempOk = (gTemperature >= 10 && gTemperature <= 35);
  bool phOk   = (gPH >= 6.5 && gPH <= 8.5);
  bool turbOk = (gTurbidity < 100);

  if (phOk && turbOk && tempOk) return "SAFE";
  if (gPH < 6.0 || gPH > 9.0 || gTurbidity > 300) return "UNSAFE";
  return "CAUTION";
}

// ─── SPIFFS logging ──────────────────────────────────────────
void ensureCSVHeader() {
  if (!SPIFFS.exists("/data.txt")) {
    File f = SPIFFS.open("/data.txt", FILE_WRITE);
    if (f) { f.println("Time,Temp,pH,Turbidity,Status"); f.close(); }
  }
}

void logToSPIFFS() {
  File f = SPIFFS.open("/data.txt", FILE_APPEND);
  if (!f) { Serial.println("[SPIFFS] Open failed"); return; }

  // Timestamp: millis in seconds (replace with RTC if available)
  unsigned long secs = millis() / 1000;
  f.printf("%lu,%.1f,%.1f,%.0f,%s\n",
           secs, gTemperature, gPH, gTurbidity, waterStatus().c_str());
  f.close();
  Serial.println("[SPIFFS] Logged");
}

// ─── HTTP POST to backend ────────────────────────────────────
void postToBackend() {
  HTTPClient http;
  http.setConnectTimeout(5000);  // 5 second connection timeout
  http.setTimeout(10000);         // 10 second total timeout
  
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  // Build JSON
  StaticJsonDocument<128> doc;
  doc["temperature"] = gTemperature;
  doc["ph"]          = gPH;
  doc["turbidity"]   = gTurbidity;
  doc["status"]      = waterStatus();

  String body;
  serializeJson(doc, body);

  Serial.print("[HTTP] Sending POST to ");
  Serial.println(SERVER_URL);
  int code = http.POST(body);
  
  if (code > 0) {
    Serial.printf("[HTTP] Response code: %d\n", code);
    Serial.printf("[HTTP] Body: %s\n", body.c_str());
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(code).c_str());
  }

  http.end();
}

// ─── WiFi connect ────────────────────────────────────────────
void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();
  
  // Set WiFi to station mode (critical for ESP32!)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // Turn off WiFi to reset
  delay(100);
  
  // Disable WiFi power saving for more reliable connection
  WiFi.setSleep(false);
  
  Serial.println("\n[WiFi] Starting connection attempt...");
  Serial.print("[WiFi] SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  int maxAttempts = 40;  // Increased from 20 (40 * 500ms = 20 seconds)
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Show progress on display every 10 attempts
    if (attempts % 10 == 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Connecting WiFi");
      display.print("Attempt: ");
      display.println(attempts);
      display.display();
    }
  }
  
  Serial.println(); // newline after dots
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] ✓ Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP().toString());
    Serial.print("[WiFi] RSSI (signal strength): ");
    Serial.println(WiFi.RSSI());
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP().toString());
    display.print("RSSI: ");
    display.println(WiFi.RSSI());
    display.display();
    delay(2000);
  } else {
    Serial.println("[WiFi] ✗ Failed to connect after 20 seconds");
    Serial.print("[WiFi] Final status code: ");
    Serial.println(WiFi.status());
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed");
    display.println("Status Code: ");
    display.println(WiFi.status());
    display.println("Offline Mode");
    display.display();
    delay(2000);
  }
}
