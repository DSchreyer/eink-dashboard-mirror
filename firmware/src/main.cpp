// Persistent WiFi-fetch firmware: periodically pulls the raw packed image
// the eink-snapshot HA add-on writes, and displays it using the confirmed
// OTP-waveform driver from today's bring-up (raw-firmware/). No ESPHome
// involved on this device at all -- the add-on does the screenshotting,
// this firmware just fetches a plain file and paints it.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "epd7in5_V2.h"

#define EPD_BUFFER_SIZE (800UL * 480UL / 8UL)  // 48000 bytes

// -- Config -----------------------------------------------------------
// Fill these in for your own network before flashing -- placeholders on
// purpose, never commit real credentials here.
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// DEVICE_NAME must match a device name configured in the eink-dashboard
// add-on's web UI (Settings -> Add-ons -> E-Ink Dashboard Mirror ->
// Open Web UI) -- the add-on supports multiple named devices, each with
// its own dashboard/settings, and writes each one's output to its own
// filename. When flashing a NEW physical panel, change this to that
// panel's device name before flashing.
static const char *DEVICE_NAME = "main";
// Point this at your own Home Assistant host.
static const char *IMAGE_URL_BASE = "http://YOUR_HA_HOST:8123/local/eink_dashboard_";
// Deliberately much shorter than the add-on's own render interval
// (10 min default): the panel only actually redraws when the fetched
// content's hash changed (see last_hash below), so polling often is
// cheap -- it's just a small HTTP GET, no visible refresh happens unless
// something real changed. This is what makes "Refresh now" in the web
// UI feel responsive instead of needing a manual power-cycle to notice a
// new frame (confirmed during bring-up: without this, a fresh render sat
// unnoticed for up to 10 minutes).
static const unsigned long FETCH_INTERVAL_MS = 60UL * 1000UL;
static const unsigned long WIFI_RETRY_MS = 15000UL;
// -----------------------------------------------------------------------

static uint8_t *frame_buffer = nullptr;
static uint32_t last_hash = 0;
static String g_status = "NOT STARTED YET";
static unsigned long g_lastRun = 0;

// Simple FNV-1a over the downloaded frame -- just to avoid re-driving the
// panel (and the ~4.4s OTP refresh + visible flicker that comes with it)
// when the dashboard hasn't actually changed since the last successful
// display. Not a security check, just a "did anything change" check.
static uint32_t fnv1a(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println("[wifi] (re)connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_RETRY_MS) {
    delay(200);
  }
  bool ok = WiFi.status() == WL_CONNECTED;
  Serial.printf("[wifi] status=%d connected=%d ip=%s\n", WiFi.status(), ok,
                ok ? WiFi.localIP().toString().c_str() : "-");
  return ok;
}

// Fetches this device's image (IMAGE_URL_BASE + DEVICE_NAME + ".bin")
// into frame_buffer. Returns true only on a complete, correctly-sized
// download -- partial/failed downloads are discarded rather than risking
// a corrupted frame reaching the panel.
static bool fetchImage() {
  String url = String(IMAGE_URL_BASE) + DEVICE_NAME + ".bin";
  HTTPClient http;
  http.begin(url);
  http.setTimeout(15000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[fetch] HTTP %d\n", code);
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t received = 0;
  unsigned long dlStart = millis();
  while (received < EPD_BUFFER_SIZE && millis() - dlStart < 15000) {
    if (http.connected() && stream->available()) {
      int n = stream->readBytes(frame_buffer + received, EPD_BUFFER_SIZE - received);
      received += n;
    } else {
      delay(10);
    }
  }
  http.end();

  if (received != EPD_BUFFER_SIZE) {
    Serial.printf("[fetch] incomplete: %u / %lu bytes\n", (unsigned)received, EPD_BUFFER_SIZE);
    return false;
  }
  return true;
}

static void displayFrame() {
  Epd epd;
  unsigned long tInit = millis();
  int initResult = epd.Init();
  if (initResult != 0) {
    Serial.println("[display] Init() failed, skipping this cycle");
    return;
  }
  Serial.printf("[display] Init() took %lums\n", millis() - tInit);

  unsigned long tDisp = millis();
  bool ok = epd.DisplayFrame(frame_buffer);
  Serial.printf("[display] DisplayFrame() took %lums, confirmed=%d\n", millis() - tDisp, ok);

  if (ok) {
    epd.Sleep();
    Serial.println("[display] Sleep() called");
  } else {
    Serial.println("[display] BUSY never confirmed -- leaving panel alone, data was still sent");
  }
}

static void runCycle() {
  if (!ensureWifi()) {
    g_status = "wifi unavailable, will retry next cycle";
    return;
  }

  if (!fetchImage()) {
    g_status = "fetch failed, will retry next cycle";
    return;
  }

  uint32_t h = fnv1a(frame_buffer, EPD_BUFFER_SIZE);
  if (h == last_hash) {
    g_status = "fetched OK, unchanged since last display -- skipping refresh";
    Serial.println("[cycle] " + g_status);
    return;
  }

  Serial.println("[cycle] content changed, refreshing panel");
  displayFrame();
  last_hash = h;
  g_status = "displayed at " + String(millis() / 1000) + "s uptime";
}

void setup() {
  Serial.begin(115200);
  delay(500);

  frame_buffer = (uint8_t *)malloc(EPD_BUFFER_SIZE);
  if (!frame_buffer) {
    Serial.println("FATAL: malloc failed");
    while (true) delay(1000);
  }

  runCycle();
  g_lastRun = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - g_lastRun >= FETCH_INTERVAL_MS) {
    g_lastRun = now;
    runCycle();
  }
  Serial.println("[status] " + g_status);
  delay(5000);
}
