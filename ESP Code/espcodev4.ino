// ============================================================
//  PulseOTA — ESP32 Firmware v0.0.4
//  4-Channel Relay control + OTA update check on boot
//  Server: https://pulseotav2.vercel.app
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
#define CURRENT_VERSION   "0.0.4"

// 4-Channel Relay GPIO Pins (Relay 1: GPIO 32, Relay 2: GPIO 33, Relay 3: GPIO 26, Relay 4: GPIO 27)
#define RELAY1_PIN        32
#define RELAY2_PIN        33
#define RELAY3_PIN        26
#define RELAY4_PIN        27

const int relayPins[4] = { RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN };
const int NUM_RELAYS = 4;

// Relay trigger logic:
// Set to HIGH if your relay module is Active-HIGH (HIGH = Relay ON, LOW = Relay OFF)
// Set to LOW if your relay module is Active-LOW (LOW = Relay ON, HIGH = Relay OFF)
#define RELAY_ON_STATE    HIGH
#define RELAY_OFF_STATE   LOW

const char* WIFI_SSID     = "testota";
const char* WIFI_PASSWORD = "12345678";

const char* VERSION_URL   = "https://pulseotav2.vercel.app/ota/version.json";

// ------------------------------------------------------------
//  RELAY SETTINGS
// ------------------------------------------------------------
#define RELAY_ON_DELAY    100
#define RELAY_OFF_DELAY   1000

// ------------------------------------------------------------
//  PERIODIC CHECK SETTINGS
// ------------------------------------------------------------
#define OTA_CHECK_INTERVAL 120000 // 2 minutes (120,000 milliseconds)
unsigned long lastOTACheck = 0;

// ------------------------------------------------------------
//  Helper — Set All Relays
// ------------------------------------------------------------
void setAllRelays(int state) {
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], state);
    if (state == RELAY_ON_STATE) {
      delay(30); // 30ms stagger prevents simultaneous coil inrush current & brownouts
    }
  }
}

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
//  OTA Update Check (Clean & Proven HTTPUpdate Architecture)
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

  // Disable all 4 relays during update for safety
  Serial.println("[OTA] Disabling all relays for safety during flash write...");
  setAllRelays(RELAY_OFF_STATE);

  WiFiClientSecure updateClient;
  updateClient.setInsecure(); // Skip SSL certificate verification

  // Show OTA download progress in Serial Monitor
  httpUpdate.onProgress([](int cur, int total) {
    static int lastProgress = -1;
    if (total > 0) {
      int progress = (cur * 100) / total;
      if (progress % 10 == 0 && progress != lastProgress) {
        Serial.printf("[OTA] Progress: %d%% (%d / %d bytes)\n", progress, cur, total);
        lastProgress = progress;
      }
    }
  });

  // Follow HTTP 307/302 redirects (required for GitHub Releases)
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

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

  // Initialize all 4 relay pins
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF_STATE);
  }
  Serial.println("[SETUP] 4-Channel Relay initialized (Pins: 32, 33, 26, 27) in OFF state.");

  // Step 1 — Connect to WiFi
  connectWiFi();

  // Step 2 — Check for OTA update on every boot
  checkOTA();
  lastOTACheck = millis();

  // Step 3 — If no update, continue with normal operation
  Serial.println("[MAIN] Starting 4-channel relay operation...");
}

// ------------------------------------------------------------
//  Main Loop — 4-Channel Relay ON/OFF cycle with periodic OTA update checks
// ------------------------------------------------------------
void loop() {
  // Check for updates periodically (non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastOTACheck >= OTA_CHECK_INTERVAL) {
    lastOTACheck = currentMillis;
    Serial.println("[OTA] Scheduled Check: 2 minutes elapsed. Triggering update check...");
    checkOTA();
  }

  Serial.println("[4-CH RELAYS] All 4 Channels ON");
  setAllRelays(RELAY_ON_STATE);
  delay(RELAY_ON_DELAY);

  Serial.println("[4-CH RELAYS] All 4 Channels OFF");
  setAllRelays(RELAY_OFF_STATE);
  delay(RELAY_OFF_DELAY);
}
