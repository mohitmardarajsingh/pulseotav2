// ============================================================
//  PulseOTA — ESP32 Firmware v0.0.1
//  Relay control + OTA update check on boot
//  Server: https://esp32update.vercel.app
// ============================================================

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------
//  CONFIG — change these
// ------------------------------------------------------------
#define CURRENT_VERSION   "0.0.1"
#define RELAY_PIN         26

const char* WIFI_SSID     = "testota";
const char* WIFI_PASSWORD = "12345678";

const char* VERSION_URL   = "https://esp32update.vercel.app/ota/version.json";

// ------------------------------------------------------------
//  RELAY SETTINGS
// ------------------------------------------------------------
#define RELAY_ON_DELAY    5000
#define RELAY_OFF_DELAY   5000

// ------------------------------------------------------------
//  PERIODIC CHECK SETTINGS
// ------------------------------------------------------------
#define OTA_CHECK_INTERVAL 120000 // 2 minutes (120,000 milliseconds)
unsigned long lastOTACheck = 0;

// ------------------------------------------------------------
//  WiFi Connect
// ------------------------------------------------------------
void connectWiFi() {
  Serial.println("[WiFi] Starting connection process...");
  Serial.print("[WiFi] SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    Serial.printf("[WiFi] Connecting... Attempt %d/20 (Status: %d)\n", attempts, WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connection successful!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("[WiFi] Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else {
    Serial.printf("[WiFi] Connection failed after %d attempts. Status: %d\n", attempts, WiFi.status());
    Serial.println("[WiFi] Skipping initial OTA check, will retry during periodic checks.");
  }
}

// ------------------------------------------------------------
//  OTA Update Check
// ------------------------------------------------------------
void checkOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[OTA] No WiFi connection (Status: %d). Skipping check.\n", WiFi.status());
    return;
  }

  Serial.println("[OTA] Starting update check...");
  Serial.printf("[OTA] Fetching metadata from URL: %s\n", VERSION_URL);

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Skip SSL certificate verification

  HTTPClient http;
  if (!http.begin(secureClient, VERSION_URL)) {
    Serial.println("[OTA] Failed to initialize connection to server.");
    return;
  }

  Serial.println("[OTA] Sending GET request for version.json...");
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("[OTA] HTTP GET request failed. HTTP Response Code: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  Serial.println("[OTA] Metadata fetched successfully.");
  Serial.printf("[OTA] version.json raw payload:\n%s\n", payload.c_str());

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.printf("[OTA] JSON parse failed. Error: %s\n", error.c_str());
    return;
  }

  const char* serverVersion = doc["version"];
  const char* firmwareUrl   = doc["url"];

  if (!serverVersion || !firmwareUrl) {
    Serial.println("[OTA] Error: JSON payload is missing 'version' or 'url' fields.");
    return;
  }

  Serial.printf("[OTA] Version info: Local Version [%s] | Server Version [%s]\n", CURRENT_VERSION, serverVersion);

  if (String(serverVersion) == String(CURRENT_VERSION)) {
    Serial.println("[OTA] System is up to date. No update needed.");
    return;
  }

  Serial.printf("[OTA] New version [%s] available! Initiating update...\n", serverVersion);
  Serial.printf("[OTA] Binary download URL: %s\n", firmwareUrl);

  // Disable relay during update for safety
  Serial.printf("[OTA] Disabling relay on pin %d for safety during flash write...\n", RELAY_PIN);
  digitalWrite(RELAY_PIN, LOW);

  WiFiClientSecure updateClient;
  updateClient.setInsecure(); // Skip SSL certificate verification

  Serial.println("[OTA] Downloading and applying firmware binary...");
  t_httpUpdate_return result = httpUpdate.update(updateClient, String(firmwareUrl));

  switch (result) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Update FAILED. Error Code (%d): %s\n",
        httpUpdate.getLastError(),
        httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] Update failed: Server returned 'no updates'.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[OTA] Update completed successfully! Rebooting device...");
      break;
  }
}

// ------------------------------------------------------------
//  Setup
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("============================================");
  Serial.print("  PulseOTA Firmware v");
  Serial.println(CURRENT_VERSION);
  Serial.println("============================================");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Step 1 — Connect to WiFi
  connectWiFi();

  // Step 2 — Check for OTA update on every boot
  checkOTA();
  lastOTACheck = millis();

  // Step 3 — If no update, continue with normal operation
  Serial.println("[MAIN] Starting relay operation...");
}

// ------------------------------------------------------------
//  Main Loop — Relay ON/OFF cycle with periodic OTA update checks
// ------------------------------------------------------------
void loop() {
  // Check for updates periodically (non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastOTACheck >= OTA_CHECK_INTERVAL) {
    lastOTACheck = currentMillis;
    Serial.println("[OTA] Scheduled Check: 2 minutes elapsed. Triggering update check...");
    checkOTA();
  }

  Serial.println("[RELAY] ON");
  digitalWrite(RELAY_PIN, HIGH);
  delay(RELAY_ON_DELAY);

  Serial.println("[RELAY] OFF");
  digitalWrite(RELAY_PIN, LOW);
  delay(RELAY_OFF_DELAY);
}