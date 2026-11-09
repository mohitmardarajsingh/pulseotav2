# PulseOTA 🚀

PulseOTA is a minimal, serverless OTA (Over-the-Air) firmware update server designed specifically for ESP32 microcontrollers. It leverages Next.js 14 and Vercel's edge network to serve firmware binaries and version configurations instantly and securely without requiring databases, dynamic APIs, or complex backend infrastructure.

---

## How It Works

1. **Version Polling**: The ESP32 is programmed to periodically query a static JSON file (`/ota/version.json`) hosted on this server.
2. **Version Evaluation**: The ESP32 parses the JSON and compares the `"version"` field with its own hardcoded local version string.
3. **Firmware Fetch**: If a newer version is available, the ESP32 requests the firmware binary from the URL specified in the `"url"` field of the JSON.
4. **Flashing & Reboot**: The ESP32 downloads the binary into its secondary partition, flashes it, verified integrity, and automatically reboots to run the new software.

---

## Folder Structure

The project relies entirely on Next.js static asset routing to serve updates directly from the filesystem.

```text
PulseOTA/
├── app/                  # Next.js App Router
│   ├── globals.css       # Core styles & color variables
│   ├── layout.tsx        # Layout, SEO meta tags, Google Fonts
│   └── page.tsx          # Minimal dark-themed admin dashboard
├── public/               # Static folder served at the domain root
│   └── ota/
│       ├── version.json  # ESP32 version definition
│       └── firmware_0.0.1.bin  # Compiled ESP32 firmware binary
├── package.json
└── tsconfig.json
```

---

## version.json Format

The version file served at `https://pulseotav2.vercel.app/ota/version.json` follows this format:

```json
{
  "version": "0.0.1",
  "url": "https://pulseotav2.vercel.app/ota/firmware_0.0.1.bin",
  "notes": "Initial release — 4-channel relay toggle every 5 seconds"
}
```

---

## How to Push a New Firmware Version

Follow these 6 steps to roll out a new firmware release:

### Step 1 — Compile firmware in Arduino IDE
Open your sketch in Arduino IDE, select **Sketch** &rarr; **Export Compiled Binary**. Look inside the sketch folder to find the compiled `.bin` file (typically named `firmware.ino.bin` or similar).

### Step 2 — Rename the .bin file
Rename the exported `.bin` file to include your new version tag:
```text
firmware_0.0.2.bin
```

### Step 3 — Copy .bin to `/public/ota/`
Place the renamed `.bin` file inside the `public/ota/` directory of your local PulseOTA project repository.

### Step 4 — Update version.json
Open `public/ota/version.json` and update the content with the new version string, target firmware download URL, and release notes:
```json
{
  "version": "0.0.2",
  "url": "https://pulseotav2.vercel.app/ota/firmware_0.0.2.bin",
  "notes": "4-Channel Relay delay: 2s ON / 0.5s OFF"
}
```

### Step 5 — Push to GitHub
Stage your new binary and JSON changes, commit, and push them to your repository:
```bash
git add .
git commit -m "OTA: bump to v0.0.2"
git push
```
Vercel will detect the push and automatically deploy the updated assets in about 30 seconds.

### Step 6 — ESP32 picks up the update
Upon the next boot or at its next scheduled polling interval, the ESP32 queries the endpoint, detects the version change (`0.0.1` &rarr; `0.0.2`), downloads the binary, flashes itself, and reboots.

---

## ESP32 Firmware Configuration

Ensure your ESP32 Arduino sketch has a defined local version constant and 4-channel relay pin definitions:

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// Version configuration
const String CURRENT_VERSION = "0.0.1";
const char* VERSION_URL = "https://pulseotav2.vercel.app/ota/version.json";

// 4-Channel Relay GPIO Pins (Relay 1: GPIO 32, Relay 2: GPIO 33, Relay 3: GPIO 26, Relay 4: GPIO 27)
#define RELAY1_PIN 32
#define RELAY2_PIN 33
#define RELAY3_PIN 26
#define RELAY4_PIN 27
const int relayPins[4] = { RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN };
```

Ensure your update function compares `CURRENT_VERSION` with the fetched version string and only calls the update execution when the cloud version is different/newer.

---

## Vercel Deployment Instructions

1. Push your local repository to GitHub/GitLab.
2. Log into your **Vercel** dashboard, click **Add New** &rarr; **Project**.
3. Import the `PulseOTA` repository.
4. Keep all default configurations (Vercel automatically detects Next.js build settings).
5. Click **Deploy**. No environment variables are required.

---

## Cloudflare DNS Setup

To route your custom domain (e.g., `pulseotav2.vercel.app` or your own custom domain) to Vercel:

1. In the **Vercel** project dashboard, go to **Settings** &rarr; **Domains**.
2. Add your custom domain (or use the auto-assigned `vercel.app` subdomain).
3. In your **Cloudflare** DNS dashboard, add the following CNAME record:
   - **Type**: `CNAME`
   - **Name**: `pulseotav2`
   - **Target**: `cname.vercel-dns.com`
   - **Proxy status**: **DNS only (Gray cloud)** &mdash; *This is critical so that the ESP32 can download files over raw SSL without Cloudflare proxies interfering with large chunks.*
4. Vercel will verify the domain configuration and auto-provision the SSL certificates.
