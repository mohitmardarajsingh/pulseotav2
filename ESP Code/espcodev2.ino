// ============================================================
//  PulseOTA — ESP32 Firmware v0.0.2
//  4-Channel Relay control + OTA update check on boot
//  Server: https://pulseotav2.vercel.app
// ============================================================

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ------------------------------------------------------------
//  CONFIG — change these
// ------------------------------------------------------------
#define CURRENT_VERSION   "0.0.2"

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
#define RELAY_ON_DELAY    2000
#define RELAY_OFF_DELAY   500

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

  // Perform direct streaming OTA update
  performOTA(String(firmwareUrl));
}

// ------------------------------------------------------------
//  Direct Stream OTA Flashing Engine (Minimal & Fast)
// ------------------------------------------------------------
void performOTA(const String& firmwareUrl) {
  Serial.printf("[OTA] Connecting to firmware server: %s\n", firmwareUrl.c_str());

  // Disable all 4 relays during update for safety
  setAllRelays(RELAY_OFF_STATE);

  // Step 1: Probe server to retrieve exact Content-Length
  int contentLength = 0;
  {
    WiFiClientSecure probeClient;
    probeClient.setInsecure();

    HTTPClient probeHttp;
    probeHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    probeHttp.setTimeout(15000);

    if (!probeHttp.begin(probeClient, firmwareUrl)) {
      Serial.println("[OTA] Error: Unable to connect to server.");
      return;
    }

    Serial.println("[OTA] Checking firmware binary size...");
    int httpCode = probeHttp.GET();
    Serial.printf("[OTA] Server HTTP Response Code: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_PARTIAL_CONTENT) {
      Serial.printf("[OTA] Failed, HTTP error code: %d\n", httpCode);
      probeHttp.end();
      return;
    }

    contentLength = probeHttp.getSize();
    probeHttp.end();
    probeClient.stop(); // Free probe TLS session immediately
  }

  Serial.printf("[OTA] Firmware binary size: %d bytes\n", contentLength);

  if (contentLength <= 0) {
    Serial.println("[OTA] Error: Invalid firmware size.");
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.printf("[OTA] Update.begin failed! Error: %s\n", Update.errorString());
    return;
  }

  Serial.println("[OTA] Downloading and flashing firmware in chunks...");

  // Step 2: Download in 256 KB chunks (aligned to 4KB sectors) to stay well under Vercel's 60s timeout
  const size_t CHUNK_SIZE = 256 * 1024; // 262,144 bytes (~12s per chunk)
  size_t totalWritten = 0;
  static uint8_t buf[2048]; // Static buffer (0 stack usage)
  int lastProgress = -1;

  while (totalWritten < (size_t)contentLength) {
    size_t chunkEnd = totalWritten + CHUNK_SIZE - 1;
    if (chunkEnd >= (size_t)contentLength) {
      chunkEnd = contentLength - 1;
    }
    size_t expectedBytes = (chunkEnd - totalWritten) + 1;

    WiFiClientSecure chunkClient;
    chunkClient.setInsecure();

    HTTPClient chunkHttp;
    chunkHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    chunkHttp.setTimeout(25000);

    if (!chunkHttp.begin(chunkClient, firmwareUrl)) {
      Serial.println("[OTA] Error: Failed to begin chunk HTTP connection.");
      Update.abort();
      return;
    }

    char rangeHeader[48];
    snprintf(rangeHeader, sizeof(rangeHeader), "bytes=%u-%u", (unsigned int)totalWritten, (unsigned int)chunkEnd);
    chunkHttp.addHeader("Range", rangeHeader);

    Serial.printf("[OTA] Requesting chunk: %s (%u bytes)...\n", rangeHeader, (unsigned int)expectedBytes);
    int httpCode = chunkHttp.GET();

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_PARTIAL_CONTENT) {
      Serial.printf("[OTA] Chunk request failed. HTTP code: %d\n", httpCode);
      chunkHttp.end();
      Update.abort();
      return;
    }

    WiFiClient* stream = chunkHttp.getStreamPtr();
    stream->setTimeout(20000); // 20-second read timeout per readBytes call

    size_t chunkRead = 0;
    while (chunkRead < expectedBytes) {
      size_t toRead = expectedBytes - chunkRead;
      if (toRead > sizeof(buf)) {
        toRead = sizeof(buf);
      }

      int r = stream->readBytes((char*)buf, toRead);
      if (r <= 0) {
        Serial.printf("\n[OTA] Error: Stream timeout waiting for chunk data (%u / %u bytes read).\n", (unsigned int)chunkRead, (unsigned int)expectedBytes);
        chunkHttp.end();
        Update.abort();
        return;
      }

      size_t w = Update.write(buf, r);
      if (w != (size_t)r) {
        Serial.println("\n[OTA] Flash write error!");
        chunkHttp.end();
        Update.abort();
        return;
      }

      chunkRead += r;
      totalWritten += r;

      int progress = (totalWritten * 100) / contentLength;
      if (progress % 10 == 0 && progress != lastProgress) {
        Serial.printf("[OTA] Progress: %d%% (%u / %d bytes)\n", progress, (unsigned int)totalWritten, contentLength);
        lastProgress = progress;
      }
    }

    chunkHttp.end();
    chunkClient.stop();
  }

  Serial.printf("[OTA] Total bytes written: %u of %d\n", (unsigned int)totalWritten, contentLength);

  if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("============================================");
      Serial.println("[OTA] UPDATE SUCCESSFUL! Rebooting device...");
      Serial.println("============================================");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("[OTA] Error: Update not finished.");
    }
  } else {
    Serial.printf("[OTA] Error: %s\n", Update.errorString());
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
