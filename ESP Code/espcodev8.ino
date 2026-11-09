// ============================================================
//  PulseOTA — ESP32 Firmware v0.0.8
//  4-Channel Relay Pattern: Knight Rider / Bounce Scanner (1->2->3->4->3->2)
//  Server: https://pulseotav2.vercel.app
// ============================================================

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ------------------------------------------------------------
//  CONFIG — change these
// ------------------------------------------------------------
#define CURRENT_VERSION   "0.0.8"

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
//  PATTERN SETTINGS — Bounce Scanner
// ------------------------------------------------------------
#define SCAN_DELAY        250  // Milliseconds each relay is illuminated in bounce order

// Bounce sequence: 1 -> 2 -> 3 -> 4 -> 3 -> 2
const int BOUNCE_SEQ[] = { 1, 2, 3, 4, 3, 2 };
const int SEQ_LENGTH = sizeof(BOUNCE_SEQ) / sizeof(BOUNCE_SEQ[0]);

// ------------------------------------------------------------
//  PERIODIC CHECK SETTINGS
// ------------------------------------------------------------
#define OTA_CHECK_INTERVAL 120000 // 2 minutes (120,000 milliseconds)
unsigned long lastOTACheck = 0;

// ------------------------------------------------------------
//  Relay Helpers
// ------------------------------------------------------------
void setAllRelays(int state) {
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], state);
  }
}

void setRelay(int channel, int state) {
  if (channel >= 1 && channel <= NUM_RELAYS) {
    digitalWrite(relayPins[channel - 1], state);
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
  secureClient.stop(); // Free TLS session buffer before downloading binary

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

  // Check if ESP32 flash partition table actually has an OTA partition
  const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
  if (!updatePartition) {
    Serial.println("[OTA] CRITICAL ERROR: No OTA partition found on ESP32!");
    Serial.println("[OTA] Reason: Arduino IDE Partition Scheme is likely set to 'Huge APP (No OTA)'.");
    Serial.println("[OTA] Fix: In Arduino IDE, change Tools -> Partition Scheme to 'Minimal SPIFFS (1.9MB APP with OTA)' or 'Default 4MB with ffat' and re-flash via USB.");
    return;
  }
  Serial.printf("[OTA] Target OTA Partition: %s (Available space: %u bytes)\n", updatePartition->label, updatePartition->size);
  Serial.printf("[OTA] Free Heap: %d bytes\n", ESP.getFreeHeap());

  // Disable all 4 relays during update for safety
  Serial.println("[OTA] Disabling all 4 relays for safety during flash write...");
  setAllRelays(RELAY_OFF_STATE);

  WiFiClientSecure updateClient;
  updateClient.setInsecure(); // Skip SSL certificate verification
  updateClient.setTimeout(30); // 30 seconds socket timeout for downloading binary

  // Follow HTTP 301/302/307/308 redirects from Vercel edge CDN
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);

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
  Serial.println("  Pattern: Bounce Scanner (1->2->3->4->3->2)");
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
  Serial.println("[MAIN] Starting Bounce Scanner pattern...");
}

// ------------------------------------------------------------
//  Main Loop — Knight Rider / Bounce Scanner
// ------------------------------------------------------------
void loop() {
  // Check for updates periodically (non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastOTACheck >= OTA_CHECK_INTERVAL) {
    lastOTACheck = currentMillis;
    Serial.println("[OTA] Scheduled Check: 2 minutes elapsed. Triggering update check...");
    checkOTA();
  }

  // Iterate through bounce sequence
  for (int i = 0; i < SEQ_LENGTH; i++) {
    int ch = BOUNCE_SEQ[i];
    Serial.printf("[BOUNCE] Relay %d ON\n", ch);
    setRelay(ch, RELAY_ON_STATE);
    delay(SCAN_DELAY);

    Serial.printf("[BOUNCE] Relay %d OFF\n", ch);
    setRelay(ch, RELAY_OFF_STATE);
  }
}
