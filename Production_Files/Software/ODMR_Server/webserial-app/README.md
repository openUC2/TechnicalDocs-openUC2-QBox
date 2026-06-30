# openUC2 ODMR — Standalone WebSerial App

A self-contained, **WiFi-free** control interface for the openUC2 ODMR device
(ESP32-S3 / ESP32-C3). It talks to the firmware entirely over the USB-CDC
**Web Serial API**, mirroring every feature of the on-device web UI:

| Tool | Mirrors | Serial commands used |
|------|---------|----------------------|
| **Frequenz-Sweep** | `messung.html` / `messung_webserial.html` | `SWEEP`, `SWEEPSTOP` |
| **B-Field Monitor** | `ratio.html` | `RATIO` |
| **Justage (Alignment)** | `justage.html` | `INTENSITY`, `GAIN`, `INTTIME`, `GETTSL` |
| Connection bar | `/version`, `/tsl/settings` | `VERSION`, `STATUS`, `GETTSL`, `PING` |

It is a plain static site (no build step) designed to be hosted on **GitHub
Pages**. All assets (Bootstrap, the shared `style.css`, the NV-lattice image)
are bundled locally, so it also works fully offline once loaded.

---

## Using it

1. Flash the firmware (any normal build works; or a dedicated serial-only build
   — see below) and connect the board by **USB**.
2. Open the app in **Google Chrome, Microsoft Edge, or Opera** on the desktop.
   *(Web Serial is not available in Firefox or Safari, or on iOS.)*
3. Click **„Gerät verbinden"** and pick the serial port (115200 baud).
4. The connection bar turns green and shows the firmware version. Use the tabs:
   - **Frequenz-Sweep** — set start/stop/step (+ averages, settle), run a sweep,
     overlay multiple series, export CSV.
   - **B-Field Monitor** — 2- or 3-point ratio monitoring with a live plot.
   - **Justage** — live photodiode intensity for optical alignment, plus TSL2591
     gain / integration-time settings.

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
HELP                          -> CMDS …
```

`f3 = 0` selects 2-point ratio mode. Gain values follow the TSL2591 register
encoding; integration time is `0..5` → 100…600 ms.

---

## Files

```
webserial-app/
├── index.html          Single-page UI (connection bar + 4 tabs)
├── assets/
│   ├── app.js          UI controller + i18n (DE/EN)
│   ├── serial.js       SerialDevice — Web Serial driver, command queue, parsing
│   ├── plot.js         LinePlot — lightweight canvas chart
│   ├── style.css       Shared UC2 stylesheet (copy of data/style.css)
│   ├── bootstrap.min.css / bootstrap.bundle.min.js
│   └── NVGitter.png
└── README.md
```

`serial.js` and `plot.js` are ES modules with no external dependencies.

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

## Serial-only firmware (optional)

The app works with the standard firmware (WiFi AP + serial both active). To
build a firmware that **disables WiFi entirely** and exposes only the serial
command interface, add the `SERIAL_ONLY_MODE` flag, e.g. in `platformio.ini`:

```ini
build_flags = -DESP32C3 -DSERIAL_ONLY_MODE
  -DCONFIG_TINYUSB_CDC_ENABLED=y
  ...
```

The status LEDs still react to serial activity (rainbow when connected, red
during a sweep, blue during alignment).
