// WiFi-fetch firmware: periodically pulls the raw packed image the
// eink-dashboard-mirror HA add-on writes, and displays it using the
// confirmed OTP-waveform driver (see waveshare-7in5-v2-esp32-fix). No
// ESPHome involved on this device at all -- the add-on does the
// screenshotting, this firmware just fetches a plain file and paints it.
//
// Two power modes, chosen with DEEP_SLEEP_ENABLED below:
//
// - false (default): stays awake and WiFi-associated the whole time,
//   polling every FETCH_INTERVAL_MS. Cheap to poll often since a fetch is
//   a small HTTP GET and only actually redraws the panel when the content
//   changed -- but continuous WiFi association draws ~80-150mA the whole
//   time, so this is a "leave it on USB power" design, not a battery one.
//   Good for a desk panel, where fast polling is what makes the add-on's
//   "Refresh now" feel instant instead of needing a manual power-cycle.
//
// - true: real deep sleep between cycles instead of a busy-poll loop.
//   Each wake is a full reset (setup() runs again from scratch, loop() is
//   never reached) -- WiFi reconnects from cold every time (~1-5s),
//   fetches once, redraws only if changed, then goes straight back to
//   sleep for however long this device's add-on config currently says
//   (fetched fresh each cycle -- see fetchSleepMinutes() -- so a battery
//   panel automatically tracks interval_minutes changed in the add-on UI
//   without a reflash; DEEP_SLEEP_FALLBACK_MINUTES below only matters if
//   that fetch fails). Deep sleep cuts power to essentially everything
//   except a small RTC memory region, so average current drops to
//   roughly what one fetch+draw cycle costs divided over the whole sleep
//   interval -- multi-day/week battery life instead of ~a day. The
//   tradeoff: it only checks for new content once per interval, not every
//   60s, so it's less "instant" than the always-on mode -- the right
//   tradeoff for a battery panel, wrong one for a USB desk panel.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sleep.h>
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

// See the big comment above for the tradeoff. Flip to true and reflash for
// a battery-powered panel; leave false for a USB-powered one.
static const bool DEEP_SLEEP_ENABLED = false;

// Only used when DEEP_SLEEP_ENABLED is true, and only as a fallback: the
// real sleep duration is fetched fresh every cycle from the add-on's own
// interval_minutes for this device (see fetchSleepMinutes()) so it can't
// silently drift out of sync the way a second hardcoded constant here
// would. This just covers the gap before that first succeeds -- no
// render yet on a brand-new device, or a transient network hiccup.
static const uint64_t DEEP_SLEEP_FALLBACK_MINUTES = 15;
// Sanity bounds on the fetched value, matching the add-on's own schema
// floor (interval_minutes: int(5,) in config.yaml) plus a generous
// ceiling -- protects against a garbled/empty response putting the panel
// to sleep for an absurd length of time.
static const uint64_t DEEP_SLEEP_MIN_MINUTES = 5;
static const uint64_t DEEP_SLEEP_MAX_MINUTES = 24UL * 60UL;

// Deliberately much shorter than the add-on's own render interval
// (10 min default): the panel only actually redraws when the fetched
// content's hash changed (see last_hash below), so polling often is
// cheap -- it's just a small HTTP GET, no visible refresh happens unless
// something real changed. This is what makes "Refresh now" in the web
// UI feel responsive instead of needing a manual power-cycle to notice a
// new frame (confirmed during bring-up: without this, a fresh render sat
// unnoticed for up to 10 minutes). Only used when DEEP_SLEEP_ENABLED is
// false.
static const unsigned long FETCH_INTERVAL_MS = 60UL * 1000UL;
static const unsigned long WIFI_RETRY_MS = 15000UL;
// -----------------------------------------------------------------------

static uint8_t *frame_buffer = nullptr;
// RTC_DATA_ATTR: lives in the small RTC memory region that survives deep
// sleep (everything else -- normal RAM, globals without this attribute --
// is wiped on every wake, since a wake from deep sleep is a full reset
// that re-runs setup() from scratch). Harmless to use this attribute in
// always-on mode too -- there it just behaves like a normal static
// surviving powered-on resets, which is what it always did.
static RTC_DATA_ATTR uint32_t last_hash = 0;
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

// Reads this device's currently configured refresh interval from the
// add-on -- a plain integer written alongside the .bin on every render
// (see the add-on's snapshot_loop.py interval_path()) rather than the
// add-on's own REST API, since that sits behind HA ingress and isn't
// reachable directly from an unauthenticated device the way HA's /local/
// static files already are. Falls back to DEEP_SLEEP_FALLBACK_MINUTES on
// any failure (no successful render yet, network hiccup, garbled/out-of-
// range response) so a bad fetch can't strand the panel asleep for the
// wrong length of time.
static uint64_t fetchSleepMinutes() {
  String url = String(IMAGE_URL_BASE) + DEVICE_NAME + "_interval.txt";
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[interval] HTTP %d, using fallback %llu min\n", code,
                  (unsigned long long)DEEP_SLEEP_FALLBACK_MINUTES);
    http.end();
    return DEEP_SLEEP_FALLBACK_MINUTES;
  }
  String body = http.getString();
  http.end();

  long parsed = body.toInt();  // 0 for empty/non-numeric
  if (parsed < (long)DEEP_SLEEP_MIN_MINUTES || parsed > (long)DEEP_SLEEP_MAX_MINUTES) {
    Serial.printf("[interval] got %ld (raw \"%s\"), out of sane range, using fallback %llu min\n",
                  parsed, body.c_str(), (unsigned long long)DEEP_SLEEP_FALLBACK_MINUTES);
    return DEEP_SLEEP_FALLBACK_MINUTES;
  }
  Serial.printf("[interval] add-on says %ld min, using that\n", parsed);
  return (uint64_t)parsed;
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

// Disconnects and powers down the WiFi radio, then enters deep sleep for
// `minutes` (see fetchSleepMinutes()). Never returns -- the next thing
// that runs is setup(), from a full reset, when the timer fires.
static void goToDeepSleep(uint64_t minutes) {
  Serial.println("[sleep] going to deep sleep for " + String((unsigned long)minutes) + " min");
  Serial.flush();  // otherwise the last line(s) can be lost -- sleep cuts
                    // power before a buffered UART write finishes
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(minutes * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
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

  if (DEEP_SLEEP_ENABLED) {
    // Fetched after runCycle() (which already brought WiFi up via
    // ensureWifi()) so this is just one more small GET on an already-
    // live connection, not a second reconnect.
    uint64_t sleepMinutes = fetchSleepMinutes();
    goToDeepSleep(sleepMinutes);  // does not return
  }

  g_lastRun = millis();
}

// Only ever reached when DEEP_SLEEP_ENABLED is false -- setup() calls
// goToDeepSleep() (which never returns) before falling through to here
// when it's true, so this whole always-on poll loop is simply unused in
// that mode.
void loop() {
  unsigned long now = millis();
  if (now - g_lastRun >= FETCH_INTERVAL_MS) {
    g_lastRun = now;
    runCycle();
  }
  Serial.println("[status] " + g_status);
  delay(5000);
}
