# ODMR Server: issues + fixes (ordered for implementation)

Target files (from your snapshot)

* Firmware: `Production_Files/Software/ODMR_Server/src/main.cpp`
* Web assets as headers: `website/*_html.h`, `website/style_css.h`
* Build/merge: `.github/workflows/build_firmware.yaml` (for single merged bin)


## P0 - connectivity + “website feels unstable” (clients drop, long stalls, CSS missing)

### 1) Captive portal handling causes periodic disconnects and extra traffic

Symptoms

* Devices “leave” the WiFi after some time (Android/iOS/iPadOS/Windows).
* Frequent background connectivity checks.

Current code

* DNS wildcard + redirects on captive endpoints:

  * `dnsServer.start(DNS_PORT, "*", 192.168.4.1)`
  * `/generate_204`, `/connecttest.txt`, `/ncsi.txt`, `/hotspot-detect.html`, etc return `302` to `http://192.168.4.1/`.

Why this hurts

* OS captive checks keep re-triggering and some devices downgrade/disconnect “no internet” networks more aggressively when they see portal redirects.

A) Stability-first (recommended): stop behaving like a captive portal

* Keep AP, but respond to OS probe endpoints with “success” responses (no redirect), so the OS stops nagging and reduces background requests.

  * Android: `/generate_204` -> HTTP 204, empty body
  * Windows: `/connecttest.txt` -> HTTP 200 body “Microsoft Connect Test”
  * Windows: `/ncsi.txt` -> HTTP 200 body “Microsoft NCSI”
  * Apple: `/hotspot-detect.html` and `/library/test/success.html` -> HTTP 200 body “Success”
* Still serve your UI at `http://192.168.4.1/` (users open it manually => we serve a little captive portal side that has a button to the website the user has to click to open it in the standard browser ).

Acceptance test

* Join WiFi on Android/iOS/Windows: device stays connected for 10+ minutes without auto-dropping.
* Browser can open `192.168.4.1` reliably.

Minimal code sketch (A)

```cpp
server.on("/generate_204", HTTP_GET, [](){ server.send(204, "text/plain", ""); });

server.on("/connecttest.txt", HTTP_GET, [](){
  server.send(200, "text/plain", "Microsoft Connect Test");
});
server.on("/ncsi.txt", HTTP_GET, [](){
  server.send(200, "text/plain", "Microsoft NCSI");
});

server.on("/hotspot-detect.html", HTTP_GET, [](){
  server.send(200, "text/html", "Success");
});
server.on("/library/test/success.html", HTTP_GET, [](){
  server.send(200, "text/html", "Success");
});
```


### 2) External CDN requests in an offline AP cause long page stalls

Symptoms

* “CSS not loaded” / layout broken / long pauses while loading.
* Happens more often in captive mini-browsers.

Current code

* Several pages include:

  * `<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>`
* In AP mode there is no internet, so this request can hang and delay other loads.

Fix

* Remove all external CDN references in all HTML headers.
* If you need dropdown/nav behavior, implement tiny local JS or rely on your CSS-only dropdown (you already have `.dropdown:hover` in `style.css`).

Acceptance test

* Load `index.html`, `messung.html`, `ratio.html`, `justage.html` on phone: first meaningful paint without multi-second stalls.

Files

* `index_html.h`, `messung_html.h`, `messung_webserial_html.h`, `justage_html.h`, `ratio_html.h` (search for `cdn.jsdelivr`)

---

### 3) Unknown asset requests are answered with HTML, breaking images and wasting bandwidth

Symptoms

* Missing images (example: `NVGitter.png`), sometimes “design broken”.
* We removed it from the header since it was too big => we want to move it to the SPIFFs 
* The website should move to SPIFFS for assets, but in the meantime this causes broken images and extra requests. (see later issue)
* Browsers keep retrying missing assets.
* Can slow down measurements due to extra requests.

Current code

* `handleFileRequest()` returns `INDEX_HTML` for any unknown path (HTTP 200 text/html).

Fix

* Serve known binary assets explicitly (see P1 issue 7).
* For unknown paths:

  * If request looks like a page navigation (Accept contains `text/html`), redirect to `/` or serve index.
  * Otherwise return 404 (or 204 for `favicon.ico`) so browsers stop retrying.

Acceptance test

* Devtools network: missing assets return 404, not HTML.
* No repeated retries for `favicon.ico`, images.

Implementation sketch

```cpp
server.on("/favicon.ico", HTTP_GET, [](){ server.send(204, "image/x-icon", ""); });

server.onNotFound([](){
  String uri = server.uri();
  if (uri.endsWith(".html") || uri == "/" ) {
    server.send_P(200, "text/html", INDEX_HTML);
    return;
  }
  server.send(404, "text/plain", "Not found");
});
```

---

## P0 - measurement reliability and “random stop” (#77) + “slow / pauses”

### 4) WiFi sweep JS can hang forever on a single failed request (#77)

Symptoms

* Sweep stops in first half, or becomes extremely slow.
* Usually caused by one request that never resolves, the `await` blocks the loop.

Current code

* `messung.html`: sweep loop does:

  * `await freqMessung(...)`
* But the WiFi `freqMessung` implementation typically uses XHR without a timeout (WebSerial version already added a timeout, WiFi version does not).

Fix

* Add per-point timeout + retry + “skip point” fallback for WiFi measurement mode.
* Never let one failed request block the full sweep.
* Log skipped points into UI (status area) and CSV as empty/NaN.

Acceptance test

* Disable WiFi briefly mid-sweep: sweep continues (skips points) and finishes.
* No permanent hang.

Implementation approach (front-end)

* Use `fetch()` + `AbortController` with a 2-3s timeout.
* Optionally retry once.

---

### 5) Add “Stop measurement” button for WiFi mode (#68)

Symptoms

* Continuous mode (“Dauermessung”) is hard to stop intuitively.
* No way to abort a sweep in progress.

Current code

* `messung.html`: loop runs until `Frequenzen.length==0`.
* Continuous mode re-triggers via `setTimeout(MessungStarten, 600)`.

Fix

* Add global `stopRequested` flag.
* Button toggles between Start and Stop.
* In sweep loop: check flag every iteration and break cleanly.
* Clear pending `setTimeout` when stopping continuous mode.

Acceptance test

* Press Stop during sweep: sweep ends within one point and UI returns to ready state.
* Continuous mode stops immediately and does not restart.

---

### 6) Per-request sensor reads can block for integration time (causes “very slow points”)

Symptoms

* “Sometimes it takes minutes for a few points.”
* “During data acquisition there are multi-second pauses.”

Likely cause

* TSL2591 integration time can be up to 600ms, and you call reads frequently:

  * `/intensity` polled every 500ms in `justage.html`
  * sweep does one `/measure` per point
* If integration time increased, calls queue/overlap and everything feels slow.

Fix (best)
A) Firmware: cache sensor values

* Run a small periodic sampler in `loop()` (for example every 50-200ms depending on integration time).
* Store last value + timestamp.
* `/intensity` and `/measure` return cached value immediately.
=> use socket or event-based updates to push new values to the front-end when they arrive, instead of polling.

B) Front-end: prevent overlapping requests

* In `justage.html`, do not send a new `/intensity` request if the previous one is still in flight.


Acceptance test

* Justage monitoring stays responsive even at 600ms integration time.
* Sweep point-to-point timing becomes consistent.

---

## P1 - WiFi channel + logging noise (#66, #70)

### 7) Random WiFi channel selection not actually applied to SoftAP (#66)

Current code

* Picks `wifiChannel = random(1, 12)` and calls `WiFi.channel(wifiChannel);`
* Starts AP using `WiFi.softAP(dynamicSSID.c_str(), PASSWORD);` (no channel parameter)
* Alternative config uses fixed channel `1`.

Fix

* Pass the chosen channel into `WiFi.softAP(ssid, pass, channel, hidden, max_conn)` and remove `WiFi.channel()`.

Acceptance test

* Serial log prints channel X and AP is actually on X (verify by scanning WiFi details).

Implementation sketch

```cpp
bool apResult = WiFi.softAP(dynamicSSID.c_str(), PASSWORD, wifiChannel, 0, 4);
```

---

### 8) “Connection reset by peer” log spam (#70)

Symptoms

* `[E][WiFiClient.cpp:429] write(): fail ... "Connection reset by peer"`

Interpretation

* Usually means the browser closed the socket while the server was writing (not necessarily a bug).
* Becomes more common when pages request external CDNs, missing assets, or long requests.

Fix

* Primary: remove CDN calls (P0 issue 2) and stop returning HTML for missing assets (P0 issue 3).
* Optional: reduce log severity, or guard writes if library allows.
* Optional: migrate to `ESPAsyncWebServer` later if you want better concurrency.

Acceptance test

* Log spam reduces significantly in normal usage.
* No functional impact.

---

## P1 - missing/incorrect endpoints + versioning

### 9) UI calls endpoints that firmware does not implement (Laser, ADF)

Symptoms

* Buttons do nothing or cause 404.
* Can also contribute to “random stops” if JS awaits something that never answers.

Current mismatch

* Web pages reference:

  * `/Laser_An`, `/Laser_Aus`
  * `/ADF_Enable`, `/ADF_Disable`
* Firmware implements:

  * `/measure`, `/intensity`, `/ratio`, `/tsl/*`, `/odmr_act`, `/webserial_check`

Fix

*  remove these UI controls (see #75) or implement the routes in firmware.


### 10) Add `/version` endpoint (pages already try to fetch it)

Current

* Pages call `fetch('/version')`, firmware has no handler, pages fall back to “ESP32 ODMR Server”.

Fix

* Include `version_info.h` and add `/version` returning JSON.

Acceptance test

* Footer shows real version, build date, git hash.

Implementation sketch

```cpp
#include "version_info.h"

server.on("/version", HTTP_GET, [](){
  String json = "{";
  json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"build_date\":\"" + String(BUILD_DATE) + "\",";
  json += "\"build_time\":\"" + String(BUILD_TIME) + "\",";
  json += "\"git_hash\":\"" + String(GIT_HASH) + "\",";
  json += "\"git_branch\":\"" + String(GIT_BRANCH) + "\"";
  json += "}";
  server.send(200, "application/json", json);
});
```

---

## P2 - UI cleanup and correctness (#75, #71, #72, #74)

### 11) Remove “Laserdiode einschalten” button (#75)

* If you do not want laser control via UI: remove button and all JS calling `/Laser_An`/`/Laser_Aus` in both `messung.html` and `messung_webserial.html`.
* Also remove any UI state that depends on laser state.

Acceptance test

* No laser button shown, no laser endpoint calls.

---

### 12) “WebSerial tab missing when opened via ‘im Netzwerk anmelden’” (#71)

What’s happening

* All pages hide the WebSerial nav item when hostname is `192.168.4.1` or contains `ODMR_`.

Why it conflicts

* The correct condition is not “what URL you used”, but “is this browser capable and is this mode intended”.

Fix

* Use the existing endpoint `/webserial_check` instead of hostname heuristics:

  * On page load: fetch `/webserial_check`
  * If `webserial_enabled=false` hide it, else show it
* Additionally, browser capability check:

  * `if (!('serial' in navigator)) hide`

Acceptance test

* WebSerial entry appears only where it works (Chromium desktop with WebSerial).
* Captive portal or mobile does not show it.

---

### 13) “Justage value capped too low” (#72)

In your snapshot, likely sources

* Measurement pages hardcode `IntMax=36000` even though raw sensor can be 65535.
* Plot auto-scale clamps to 36000.
* Justage page uses 65535.

Fix

* Decide one consistent raw range:

  * If you truly want full 16-bit range, update `messung.html` and `messung_webserial.html` constants `IntMax` and any `Math.min(..., 36000)` clamps.
  * If 36000 is intentional (sensor saturates earlier in your typical configs), then also update Justage page’s “Messbereich” card to reflect that and explain it.

Acceptance test

* Reported cap is explainable and consistent across pages.
* No confusing mismatch (Justage shows 65535 while plots clamp at 36000).

---

### 14) Gain/integration tweaks (#74)

UI improvements

* In Justage: update “Messbereich” hints dynamically after loading `/tsl/settings`.
* Prevent setting combinations that make the UI unusably slow:

  * Example: if polling every 500ms and integration time is 600ms, auto-adjust polling to 700-800ms.

Firmware improvements

* Consider adding lower integration times if the library supports them (TSL2591 supports specific discrete values; if you need below 100ms, you may need a different sensor or a different read mode).
* Or reduce `averages` / `settle_ms` defaults where appropriate.

Acceptance test

* Changing gain/integration produces immediate, predictable changes.
* No accidental “slow mode” without warning.

---

## P1/P2 - NV image storage (#67)

### 15) NVGitter image not served, and “header image causes boot loop” (#67)

Current

* `index.html` references `NVGitter.png`, but firmware does not serve it.
* If you tried embedding raw PNG as `const char[]`, `send_P` will break on embedded null bytes.

=> Introduce Filesystem (SPIFFS/LittleFS) but still one merged bin

* Add FS partition in `custom_partition_*.csv`
* Build FS image in CI (`pio run -t buildfs`)
* Merge into the single output binary in `build_firmware.yaml` using `esptool merge_bin` at the FS partition offset
* Still flash one file via ESP Web Tools (manifest stays “single part at offset 0”)

Acceptance test

* Image loads on all devices, no retries, no boot loops.



## P2 - content correctness (partners not mentioned)

### 16) Add QuantumMiniLabs + partners to “infos.html” and/or footer

* Update `infos.html` content (your snapshot references `infos_html.h` but that file was not included here).
* Add at minimum: project name, funding/partners list, links, and contact.

Acceptance test

* “Weitere Infos” page includes the full partner list.

---

## Quick “diff checklist” (what to touch first)

1. Remove all external CDN script tags from all HTML headers.
2. Fix captive portal endpoints: return “success” responses, not redirects (or make it configurable).
3. Stop returning `INDEX_HTML` for every unknown path; return 404 for non-HTML resources; add `/favicon.ico` 204.
4. Implement stop + timeouts in WiFi sweep JS (messung.html).
5. Fix SoftAP channel selection: pass random channel into `WiFi.softAP(...)`.
6. Add `/version` endpoint from `version_info.h`.
7. Decide and unify intensity range (36000 vs 65535) across pages.
8. Serve NV image via base64 or binary header route.

---

Note on files

* The “infos_html.h” referenced by `main.cpp` is not in the uploaded set I received here. If you want exact edits for that page, re-upload that header (or the original `infos.html`).
