# openUC2 ODMR — web app

The control interface for the openUC2 ODMR device (ESP32-S3 / ESP32-C3), the
same files in every place:

| Where | Talks to the board via |
|---|---|
| <https://youseetoo.github.io/odmr>, Chrome/Edge on a computer, USB cable | Web Serial (`assets/serial.js`) |
| `http://192.168.4.1`, the board's WiFi, any browser | HTTP API (`assets/http.js`) |
| `http://192.168.7.1`, USB cable to iPad/iPhone (XIAO ESP32-S3) | HTTP API (`assets/http.js`) |

Plain `http` means the board serves the app, so it uses HTTP; Web Serial needs
`https` or `localhost` (`?board` forces HTTP, for testing against a mock).

| Tab | Serial commands / HTTP endpoints |
|-----|----------------------|
| **Anleitung (Guide)** — 16 steps to build and align the setup, animated top view (`assets/guide.js`) | live intensity from the alignment tab |
| **Frequenz-Sweep** | `SWEEP`, `SWEEPSTOP` / `/sweep` (SSE), `/sweep_stop` |
| **Live Monitor** | `MEASURE` / `/measure` |
| **B-Field Monitor** | `RATIO` / `/ratio` |
| **Justage (Alignment)** | `INTENSITY`, `GAIN`, `INTTIME`, `GETTSL` / `/intensity`, `/tsl/*` |
| Connection bar | `VERSION`, `GETTSL` / `/version`, `/tsl/settings` |

`infos.html` has the background (how ODMR works, project partners).

Live at **<https://youseetoo.github.io/odmr>**. On Android phones and tablets
the [UC2 Control app](https://github.com/youseetoo/UC2Control/releases/latest)
has the same tools natively in its **ODMR** tab (USB-C OTG cable to the phone).

It is a plain static site (no build step). All assets (Bootstrap, `style.css`)
are bundled locally, so it also works offline from the board.

---

## Using it

1. Flash the firmware (any normal build works; or a dedicated serial-only build
   — see below) and connect the board by **USB**.
2. Open the app in **Google Chrome, Microsoft Edge, or Opera** on the desktop.
   *(Web Serial is not available in Firefox or Safari, or on iOS.)*
   On Android, use the UC2 Control app instead (Chrome on Android has no Web
   Serial): open its **ODMR** tab.
3. Click **„Gerät verbinden"** and pick the serial port (115200 baud).
4. The connection bar turns green and shows the firmware version. Use the tabs:
   - **Frequenz-Sweep** — set start/stop/step (+ averages, settle), see the
     estimated duration, run a sweep, overlay multiple series (kept across
     reloads, last 20), fixed y-range or **↕ Auto** (data min–max), export CSV
     or PNG, share.
   - **Live Monitor** — intensity at a fixed frequency over time, y-range auto
     or full (0 – maximum for the current gain / integration time), CSV export.
   - **B-Field Monitor** — 2- or 3-point ratio monitoring with a live plot and
     selectable y-range.
   - **Justage** — live photodiode intensity with plot, the optimal range for
     the current gain / integration time (saturation warning), plus TSL2591
     gain / integration-time settings.
   - **Anleitung** — the setup and alignment guide; steps that need the signal
     show the live intensity right there.

   Tap or hover a plot for the nearest point (frequency · intensity).
   Downloads go to the browser's download folder.

The **„Log anzeigen"** button reveals the raw serial traffic and a box to send
manual commands (e.g. `PING`, `STATUS`).

> The previously granted port is re-opened automatically on the next visit — no
> need to re-pick it every time.

---

## Serial protocol

One command per line, `\n`-terminated, keyword is case-insensitive. The full
reference also lives in [`../SERIAL_PROTOCOL.md`](../SERIAL_PROTOCOL.md) and is
implemented in `processSerialLine()` in `src/main.cpp`.

```
PING                          -> PONG
VERSION                       -> VERSION {"version":…,"build_date":…,"git_hash":…}
STATUS                        -> STATUS  {"clients":…,"fmin":…,"fmax":…,"sweep":…}
MEASURE <f>                   -> DATA <f> <intensity> <bfield>
INTENSITY                     -> INT <intensity>           (fast cached read)
RATIO <f1> <f2> <f3|0> <avg>  -> RATIO {"avg":…,"points":[…],"r12":…,"r13":…,"r23":…}
SWEEP <fb> <fe> <fs> [avg] [settle]
                              -> SWEEP START {…}
                                 SWEEP DATA <idx> <total> <f> <intensity>
                                 SWEEP DONE <count>   |   SWEEP STOP <count>
SWEEPSTOP                     -> stops a running sweep
GAIN <0x00|0x10|0x20|0x30>    -> OK GAIN 0xXX
INTTIME <0..5>                -> OK INTTIME <v>
GETTSL                        -> TSL {"gain":…,"integration_time":…}
ADFON / ADFOFF                -> OK ADF ON|OFF
WIFI [ON|OFF]                 -> WIFI {"enabled":…,"active":…,"ssid":…,"clients":…}
HELP                          -> CMDS …
```

`f3 = 0` selects 2-point ratio mode. Gain values follow the TSL2591 register
encoding; integration time is `0..5` → 100…600 ms.

---

## Files

```
webserial-app/
├── index.html          Single-page UI (connection bar + 6 tabs, guide scene SVG)
├── infos.html          Background: ODMR, NV centres, partners
├── assets/
│   ├── app.js          UI controller + i18n (DE/EN)
│   ├── serial.js       SerialDevice — Web Serial driver, command queue, parsing
│   ├── http.js         HttpDevice — same interface over the board's HTTP API
│   ├── plot.js         LinePlot — lightweight canvas chart
│   ├── guide.js        Guide steps (DE/EN) and which scene parts each shows
│   ├── style.css       UC2 stylesheet
│   ├── logo-*.svg      Partner logos (Info tab, infos.html), local so they work offline
│   └── bootstrap.min.css / bootstrap.bundle.min.js
└── README.md
```

`serial.js` and `plot.js` are ES modules with no external dependencies.

The board serves this folder at `/` (gzipped into SPIFFS by
`scripts/gzip_assets.py`). There manual commands in the log are hidden: they
need Web Serial.

---

## Deploying to GitHub Pages

A workflow at [`.github/workflows/deploy-webserial.yml`](../../../../.github/workflows/deploy-webserial.yml)
publishes this folder automatically.

**One-time setup:** in the repository, go to **Settings ▸ Pages** and set
**Source = "GitHub Actions"**. After the next push to `main` that touches
`webserial-app/`, the site is live at:

```
https://openuc2.github.io/TechnicalDocs-openUC2-QBox/
```

You can also trigger it manually from the **Actions** tab (workflow_dispatch).

[`build_firmware.yaml`](../../../../.github/workflows/build_firmware.yaml) also
copies this folder to [youseetoo.github.io/odmr](https://youseetoo.github.io/odmr)
together with the firmware images.

---

## Local development

It is static — serve the folder with any HTTP server (ES modules require
`http://`, not `file://`):

```bash
cd Production_Files/Software/ODMR_Server/webserial-app
python3 -m http.server 8753
# open http://localhost:8753
```

---

## WiFi switch

WiFi is off by default (CE). With a device connected, the connection bar shows
**📶 WiFi off/on**; clicking it asks for confirmation (with the CE warning) and
sends `WIFI ON|OFF` (serial) or `POST /wifi` (HTTP). The board keeps the
setting across restarts. Firmware without the command: the button stays hidden.
