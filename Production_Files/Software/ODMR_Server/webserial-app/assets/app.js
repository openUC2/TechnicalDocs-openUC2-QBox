/* ===========================================================================
 * openUC2 ODMR — WebSerial app controller
 * Wires the tabbed UI (sweep, live monitor, ratio/B-field, alignment) to the
 * SerialDevice. The Android app "UC2 Control" has the same tools natively (ODMR tab).
 * =========================================================================== */
import { SerialDevice } from "./serial.js";
import { HttpDevice } from "./http.js";
import { LinePlot } from "./plot.js";
import { SCENES, STEPS } from "./guide.js";

const $ = (id) => document.getElementById(id);
// The same app runs on https://youseetoo.github.io/odmr (Web Serial) and on the
// board itself, http://192.168.4.1 over WiFi or http://192.168.7.1 over the USB
// network. Web Serial needs https or localhost, so plain http means the board
// (?board forces it, for testing against a local mock).
const ON_BOARD = !window.isSecureContext || new URLSearchParams(location.search).has("board");
const VIA_USB_NET = location.hostname.startsWith("192.168.7.");
const device = ON_BOARD ? new HttpDevice() : new SerialDevice();

/* ---------------------------------------------------------------------------
 * i18n
 * ------------------------------------------------------------------------- */
const I18N = {
  de: {
    connect: "Gerät verbinden", disconnect: "Trennen", not_connected: "Nicht verbunden",
    connected: "Verbunden", connecting: "Verbinde…", show_log: "Log anzeigen", hide_log: "Log verbergen",
    send: "Senden", clear_log: "Log leeren",
    no_serial: "Dieser Browser unterstützt die Web Serial API nicht. Bitte Chrome, Edge oder Opera am Desktop verwenden.",
    tab_sweep: "Frequenz-Sweep", tab_ratio: "B-Field Monitor", tab_align: "Justage", tab_info: "Infos",
    sweep_plot: "📈 Intensität vs. Frequenz", clear: "Leeren", sweep_params: "⚙️ Sweep-Parameter",
    f_begin: "Startfrequenz (MHz)", f_end: "Endfrequenz (MHz)", f_step: "Schrittweite (MHz)",
    averages: "Mittelungen", settle: "Settle (ms)", start_sweep: "▶ Sweep starten", stop_sweep: "⏹ Sweep stoppen",
    ready: "Bereit", running: "Läuft…", stopped: "Gestoppt", done: "Fertig",
    sweep_history: "🗂 Messreihen", col_show: "Zeigen", col_points: "Punkte", no_series: "Noch keine Messreihen.",
    card_freq: "📡 Frequenz-Einstellungen", mode_label: "Modus:", mode_2pt: "2-Punkt", mode_3pt: "3-Punkt",
    interval: "Messintervall (ms)", card_control: "🎮 Steuerung", start_monitor: "▶ Monitoring starten",
    stop_monitor: "⏹ Stoppen", measurements: "Messungen", errors: "Fehler", card_values: "📊 Aktuelle Werte",
    card_plot: "📈 Live-Plot", ratio_help: "Das normalisierte Verhältnis r = (I₁-I₂)/(I₁+I₂) ist proportional zur Magnetfeldverschiebung. f₁ und f₂ symmetrisch um das ODMR-Dip-Minimum wählen.",
    align_live: "Photodioden-Intensität (Live)", current_value: "Aktueller Wert", intensity_display: "Intensitäts-Anzeige",
    start_align: "Start Überwachung", stop_align: "Stop", status: "Status",
    wifi_on: "WLAN an", wifi_off: "WLAN aus",
    wifi_confirm_on: "WLAN des Geräts einschalten?\n\nAchtung: Das Gerät ist mit ausgeschaltetem Funk CE-konform. Mit eingeschaltetem WLAN gilt diese Konformitätsbewertung nicht mehr – Betrieb auf eigene Verantwortung.\n\nDas WLAN bleibt auch nach einem Neustart an, bis du es hier wieder ausschaltest. Danach mit dem WLAN „openUC2_ODMR_…“ verbinden und http://192.168.4.1 öffnen.",
    wifi_confirm_off: "WLAN des Geräts ausschalten? Alle über WLAN verbundenen Geräte verlieren die Verbindung.",
    wifi_title: "WLAN-Zugangspunkt des Geräts (standardmäßig aus)",
    align_hint: "Optischen Pfad für maximale Intensität justieren.", align_guide: "📘 Schritt-für-Schritt-Anleitung", range: "Messbereich", sensor_settings: "Sensor-Einstellungen",
    gain: "Verstärkung (Gain)", integration: "Integrationszeit", apply: "Einstellungen anwenden",
    optimal: "Optimal", good: "Gut", adjust: "Justage erforderlich", monitoring: "Überwachung aktiv",
    applied: "Einstellungen angewendet", applying: "Wird angewendet…", apply_err: "Fehler beim Anwenden",
    protocol_title: "Serielles Protokoll",
    col_cmd: "Befehl", col_resp: "Antwort", no_serial_title: "Web Serial wird nicht unterstützt",
    tab_live: "Live-Monitor", live_plot: "📈 Intensität vs. Zeit", live_control: "🎛 Live-Kontrolle",
    live_freq: "Frequenz (MHz)", start_live: "▶ Live-Monitor starten", stop_live: "⏹ Live-Monitor stoppen",
    live_hint: "Misst die Fluoreszenz fortlaufend bei einer festen Mikrowellenfrequenz, z. B. um die Wirkung eines Magneten direkt zu sehen.",
    y_min: "Y min", y_max: "Y max", points: "Punkte", duration: "Dauer ca.", max_points: "max. 600 Punkte",
    share: "Teilen", no_data: "Keine Daten.", sweep_invalid: "Startfrequenz < Endfrequenz und Schrittweite > 0 erforderlich.",
    align_plot: "📈 Intensitäts-Verlauf", y_stretch: "Stretch (Min–Max)", y_full: "Voll (0–Max)",
    range_hint: "Hängt von Verstärkung und Integrationszeit ab.", saturated: "Übersteuert – Gain senken",
    prev: "← Zurück", next: "Weiter →", fit: "↕ Auto",
    tab_guide: "Anleitung", guide_header: "📘 Aufbau & Justage", step: "Schritt",
    g_mirror: "Spiegel 45°", g_dichroic: "Strahlteiler", g_stage: "Diamant", g_pd: "Photodiode", g_filter: "Rotfilter",
    goto_tab_align: "Zu den Sensor-Einstellungen →", goto_tab_sweep: "Zum Frequenz-Sweep →", goto_tab_ratio: "Zum B-Field Monitor →",
    mode_serial: "WebSerial", mode_wifi: "WLAN", mode_usb: "USB-Netzwerk",
    info_title: "Über diese App", info_intro: "Dieselbe App steuert das ODMR-Gerät auf drei Wegen:",
    info_way_serial: "USB-Kabel am Computer: https://youseetoo.github.io/odmr in Chrome oder Edge öffnen, „Gerät verbinden“ klicken und den Port wählen (Web Serial).",
    info_way_wifi: "WLAN (ab Werk aus): erst per USB verbinden und oben „📶 WLAN aus“ antippen. Dann mit dem WLAN openUC2_ODMR_… verbinden und http://192.168.4.1 öffnen. Geht mit jedem Browser.",
    partners_title: "Projekt & Partner",
    partners_text: "Entwickelt im Verbundprojekt QuantumMiniLabs, gefördert vom Bundesministerium für Forschung, Technologie und Raumfahrt (BMFTR). Ziel: 100 Lernorte in Deutschland mit günstigen Quantenexperimenten aus Open Hardware ausstatten.",
    refs_title: "Weiterführende Links", ref_docs: "Anleitung zum ODMR-Experiment (openUC2 Docs)",
    ref_press: "Pressemitteilung zum Projektstart (idw)", ref_infos: "Mehr über NV-Zentren und das Experiment",
    offline_note: "Die App läuft komplett offline vom Gerät; nur die Links nach außen brauchen Internet.",
    info_way_usb: "USB-Kabel am iPad/iPhone (XIAO ESP32-S3): das Gerät erscheint als kabelgebundenes Netzwerk, http://192.168.7.1 in Safari öffnen.",
    odmr_title: "ODMR-Prinzip",
    odmr_text: "NV-Zentren in Diamant zeigen spinabhängige Fluoreszenz. Mikrowellen bei der Resonanzfrequenz (~2,87 GHz) senken die Fluoreszenz messbar. Externe Magnetfelder verschieben die Resonanz über den Zeeman-Effekt – die Basis der Magnetfeldmessung.",
    safety_title: "⚠️ Sicherheit:", safety_text: "Nie direkt in den Laser blicken. Vorsicht mit Magneten bei Implantaten/Elektronik.",
    android_title: "Android:", android_link: "App herunterladen",
    android_text: "Dieselben Messungen gibt es im ODMR-Tab der App „UC2 Control“: Board per USB-OTG anschließen, Messungen landen als Dateien auf dem Handy und lassen sich teilen.",
  },
  en: {
    connect: "Connect device", disconnect: "Disconnect", not_connected: "Not connected",
    connected: "Connected", connecting: "Connecting…", show_log: "Show log", hide_log: "Hide log",
    send: "Send", clear_log: "Clear log",
    no_serial: "This browser does not support the Web Serial API. Please use desktop Chrome, Edge or Opera.",
    tab_sweep: "Frequency Sweep", tab_ratio: "B-Field Monitor", tab_align: "Alignment", tab_info: "Info",
    sweep_plot: "📈 Intensity vs. Frequency", clear: "Clear", sweep_params: "⚙️ Sweep Parameters",
    f_begin: "Start frequency (MHz)", f_end: "End frequency (MHz)", f_step: "Step size (MHz)",
    averages: "Averages", settle: "Settle (ms)", start_sweep: "▶ Start sweep", stop_sweep: "⏹ Stop sweep",
    ready: "Ready", running: "Running…", stopped: "Stopped", done: "Done",
    sweep_history: "🗂 Measurement series", col_show: "Show", col_points: "Points", no_series: "No series yet.",
    card_freq: "📡 Frequency Settings", mode_label: "Mode:", mode_2pt: "2-Point", mode_3pt: "3-Point",
    interval: "Interval (ms)", card_control: "🎮 Control", start_monitor: "▶ Start monitoring",
    stop_monitor: "⏹ Stop", measurements: "Measurements", errors: "Errors", card_values: "📊 Current Values",
    card_plot: "📈 Live Plot", ratio_help: "The normalised ratio r = (I₁-I₂)/(I₁+I₂) is proportional to the magnetic field shift. Choose f₁ and f₂ symmetrically around the ODMR dip minimum.",
    align_live: "Photodiode Intensity (Live)", current_value: "Current value", intensity_display: "Intensity Display",
    start_align: "Start monitoring", stop_align: "Stop", status: "Status",
    wifi_on: "WiFi on", wifi_off: "WiFi off",
    wifi_confirm_on: "Switch on the device's WiFi?\n\nNote: the device is CE compliant with its radio switched off. With WiFi on, that conformity assessment no longer applies – operate at your own responsibility.\n\nWiFi stays on after a restart until you switch it off here. Then join the network \u201copenUC2_ODMR_…\u201d and open http://192.168.4.1.",
    wifi_confirm_off: "Switch off the device's WiFi? Everything connected over WiFi loses its connection.",
    wifi_title: "The device's WiFi access point (off by default)",
    align_hint: "Adjust the optical path for maximum intensity.", align_guide: "📘 Step-by-step guide", range: "Measurement Range", sensor_settings: "Sensor Settings",
    gain: "Gain", integration: "Integration time", apply: "Apply settings",
    optimal: "Optimal", good: "Good", adjust: "Adjustment required", monitoring: "Monitoring active",
    applied: "Settings applied", applying: "Applying…", apply_err: "Failed to apply",
    protocol_title: "Serial Protocol",
    col_cmd: "Command", col_resp: "Response", no_serial_title: "Web Serial not supported",
    tab_live: "Live Monitor", live_plot: "📈 Intensity vs. Time", live_control: "🎛 Live Control",
    live_freq: "Frequency (MHz)", start_live: "▶ Start live monitor", stop_live: "⏹ Stop live monitor",
    live_hint: "Measures the fluorescence continuously at one fixed microwave frequency, e.g. to watch the effect of a magnet directly.",
    y_min: "Y min", y_max: "Y max", points: "points", duration: "takes about", max_points: "max. 600 points",
    share: "Share", no_data: "No data.", sweep_invalid: "Start frequency < end frequency and step > 0 required.",
    align_plot: "📈 Intensity History", y_stretch: "Stretch (min–max)", y_full: "Full (0–max)",
    range_hint: "Depends on gain and integration time.", saturated: "Saturated – lower the gain",
    prev: "← Back", next: "Next →", fit: "↕ Auto",
    tab_guide: "Guide", guide_header: "📘 Setup & alignment", step: "Step",
    g_mirror: "Mirror 45°", g_dichroic: "Beam splitter", g_stage: "Diamond", g_pd: "Photodiode", g_filter: "Red filter",
    goto_tab_align: "To the sensor settings →", goto_tab_sweep: "To the frequency sweep →", goto_tab_ratio: "To the B-field monitor →",
    mode_serial: "WebSerial", mode_wifi: "WiFi", mode_usb: "USB network",
    info_title: "About this app", info_intro: "The same app controls the ODMR device in three ways:",
    info_way_serial: "USB cable to a computer: open https://youseetoo.github.io/odmr in Chrome or Edge, click \"Connect device\" and pick the port (Web Serial).",
    info_way_wifi: "WiFi (off by default): connect over USB first and tap “📶 WiFi off” at the top. Then join the network openUC2_ODMR_… and open http://192.168.4.1. Works in any browser.",
    partners_title: "Project & partners",
    partners_text: "Developed in the joint project QuantumMiniLabs, funded by the German Federal Ministry of Research, Technology and Space (BMFTR). Goal: equip 100 learning sites in Germany with low-cost open-hardware quantum experiments.",
    refs_title: "Further reading", ref_docs: "ODMR experiment instructions (openUC2 Docs)",
    ref_press: "Press release on the project start (idw, German)", ref_infos: "More about NV centres and the experiment",
    offline_note: "The app runs fully offline from the device; only the outbound links need internet.",
    info_way_usb: "USB cable to an iPad/iPhone (XIAO ESP32-S3): the device shows up as a wired network; open http://192.168.7.1 in Safari.",
    odmr_title: "ODMR principle",
    odmr_text: "NV centres in diamond show spin-dependent fluorescence. Microwaves at the resonance frequency (~2.87 GHz) lower the fluorescence measurably. External magnetic fields shift the resonance via the Zeeman effect – the basis of the magnetic field measurement.",
    safety_title: "⚠️ Safety:", safety_text: "Never look directly into the laser. Take care with magnets near implants/electronics.",
    android_title: "Android:", android_link: "Download the app",
    android_text: "The same measurements are in the ODMR tab of the \"UC2 Control\" app: connect the board via USB-OTG; measurements are saved as files on the phone and can be shared.",
  },
};
let lang = localStorage.getItem("language") || "de";
const t = (key) => (I18N[lang] && I18N[lang][key]) || (I18N.de[key] || key);

function updateLanguage() {
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    const k = el.getAttribute("data-i18n");
    if (I18N[lang] && I18N[lang][k] !== undefined) el.textContent = I18N[lang][k];
  });
  const dd = $("langDropdown");
  if (dd) dd.innerHTML = `🌐 ${lang.toUpperCase()}`;
  refreshDynamicLabels();
}
window.setLanguage = function (l) {
  lang = l;
  localStorage.setItem("language", l);
  updateLanguage();
};

/* ---------------------------------------------------------------------------
 * Serial log
 * ------------------------------------------------------------------------- */
const logEl = $("serialLog");
function log(line, cls = "") {
  const ts = new Date().toLocaleTimeString();
  const div = document.createElement("div");
  if (cls) div.className = cls;
  div.textContent = `${ts}  ${line}`;
  logEl.appendChild(div);
  while (logEl.children.length > 300) logEl.removeChild(logEl.firstChild);
  logEl.scrollTop = logEl.scrollHeight;
}
device.onLine((line) => {
  const cls = line.startsWith("ERR") || line.includes("ERR") ? "err" : "";
  log("← " + line, cls);
});

/* ---------------------------------------------------------------------------
 * Connection management
 * ------------------------------------------------------------------------- */
let deviceConnected = false;

function setControlsEnabled(on) {
  deviceConnected = on;
  $("sweepStart").disabled = !on;
  $("liveStart").disabled = !on;
  $("ratioStart").disabled = !on;
  $("alignStart").disabled = !on;
  $("guideLiveBtn").disabled = !on;
  $("applyTsl").disabled = !on;
  $("btnDisconnect").disabled = !on;
  $("btnConnect").disabled = on;
}

device.onStateChange(async (connected) => {
  const dot = $("connDot");
  const txt = $("connText");
  if (connected) {
    dot.className = "conn-dot connected";
    txt.textContent = t("connected");
    setControlsEnabled(true);
    // Fetch version + current TSL settings (best effort)
    try {
      const v = await device.getVersion();
      $("deviceVersion").textContent = `v${v.version} · ${v.build_date} · ${v.git_hash}`;
    } catch (e) { log("! version: " + e.message, "err"); }
    try {
      const s = await device.getTSL();
      $("gainSelect").value = "0x" + s.gain.toString(16).toUpperCase().padStart(2, "0");
      $("intSelect").value = "0x" + s.integration_time.toString(16).toUpperCase().padStart(2, "0");
      updateAlignRange();
      updateSweepSummary();
    } catch (e) { /* sensor may be absent */ }
    try { showWifi(await device.wifi()); } catch (e) { showWifi(null); } // older firmware: no switch
  } else {
    dot.className = "conn-dot";
    txt.textContent = t("not_connected");
    $("deviceVersion").textContent = "";
    showWifi(null);
    setControlsEnabled(false);
    stopSweepUI(); stopLive(); stopRatio(); stopAlign();
  }
});

/* ---- WiFi switch (radio off by default, CE) ---------------------------- */
let wifiState = null;
function showWifi(w) {
  wifiState = w;
  const b = $("btnWifi");
  b.classList.toggle("d-none", !w);
  if (!w) return;
  b.title = t("wifi_title");
  b.classList.toggle("btn-warning", w.enabled);
  b.classList.toggle("btn-outline-secondary", !w.enabled);
  b.textContent = "📶 " + (w.enabled ? `${t("wifi_on")} · ${w.ssid}` : t("wifi_off"));
}
$("btnWifi").addEventListener("click", async () => {
  const on = !wifiState.enabled;
  if (!confirm(t(on ? "wifi_confirm_on" : "wifi_confirm_off"))) return;
  try { showWifi(await device.wifi(on)); } catch (e) { log("! wifi: " + e.message, "err"); }
});

$("btnConnect").addEventListener("click", async () => {
  try {
    $("connText").textContent = t("connecting");
    await device.connect();
    log("✓ port opened @115200", "muted");
  } catch (e) {
    $("connText").textContent = t("not_connected");
    $("connDot").className = "conn-dot error";
    log("! connect: " + e.message, "err");
  }
});
$("btnDisconnect").addEventListener("click", () => device.disconnect());

// Manual command + log controls
$("btnToggleLog").addEventListener("click", () => {
  const w = $("logWrap");
  const show = w.style.display === "none";
  w.style.display = show ? "block" : "none";
  $("btnToggleLog").textContent = show ? t("hide_log") : t("show_log");
});
$("btnClearLog").addEventListener("click", () => (logEl.innerHTML = ""));
$("btnSendCmd").addEventListener("click", sendManual);
$("manualCmd").addEventListener("keydown", (e) => { if (e.key === "Enter") sendManual(); });
async function sendManual() {
  const cmd = $("manualCmd").value.trim();
  if (!cmd || !device.connected) return;
  log("→ " + cmd, "tx");
  await device.writeRaw(cmd + "\n").catch((e) => log("! " + e.message, "err"));
  $("manualCmd").value = "";
}

/* ===========================================================================
 * SWEEP TAB
 * ========================================================================= */
const sweepPlot = new LinePlot($("sweepCanvas"), {
  xLabel: "f / MHz", yLabel: "I", emptyText: "—",
  xTickFmt: (v) => v.toFixed(0), yTickFmt: (v) => Math.round(v).toString(),
  pointFmt: (p) => `${p.x.toFixed(1)} MHz · ${Math.round(p.y)}`,
});
let sweepLive = [];           // current sweep points {x,y}
let sweepSeries = [];         // [{points, color, visible, fb, fe, fs}]
let sweepRunning = false;
const SERIES_COLORS = ["#023773", "#dc3545", "#1f9c7c", "#e08e0b", "#7048b6", "#0a8a4a"];

// Measurement series survive a reload (last 20 kept).
const SWEEP_STORE = "odmr_sweep_series";
function saveSweeps() {
  try { localStorage.setItem(SWEEP_STORE, JSON.stringify(sweepSeries.slice(-20))); } catch (e) { /* storage full or blocked */ }
}
function loadSweeps() {
  try { sweepSeries = JSON.parse(localStorage.getItem(SWEEP_STORE)) || []; } catch (e) { sweepSeries = []; }
}

function renderSweepPlot() {
  const yMin = parseFloat($("sweepYMin").value), yMax = parseFloat($("sweepYMax").value);
  sweepPlot.opts.yRange = yMin < yMax ? [yMin, yMax] : null;   // both set: manual, else auto
  const series = sweepSeries
    .filter((s) => s.visible)
    .map((s) => ({ points: s.points, color: s.color, marker: false }));
  if (sweepRunning || sweepLive.length) {
    series.push({ points: sweepLive, color: "#000", dots: true });
  }
  sweepPlot.setSeries(series);
}

$("sweepStart").addEventListener("click", async () => {
  if (sweepRunning) { device.stopSweep(); return; }
  const fb = parseFloat($("fBegin").value);
  const fe = parseFloat($("fEnd").value);
  const fs = parseFloat($("fStep").value);
  const avg = parseInt($("sweepAvg").value, 10) || 1;
  const settle = parseInt($("sweepSettle").value, 10) || 10;
  if (!(fb < fe) || fs <= 0) { alert(t("sweep_invalid")); return; }

  sweepRunning = true;
  sweepLive = [];
  $("sweepStart").textContent = t("stop_sweep");
  $("sweepStart").className = "btn btn-danger w-100 mb-2";
  setSweepStatus(t("running"));

  try {
    const res = await device.sweep(fb, fe, fs, avg, settle, {
      onStart: (info) => setSweepStatus(`${t("running")} 0/${info.total}`),
      onPoint: (idx, total, f, I) => {
        sweepLive.push({ x: f, y: I });
        setSweepStatus(`${t("running")} ${idx + 1}/${total}`);
        renderSweepPlot();
      },
    });
    if (sweepLive.length) commitSweepSeries(fb, fe, fs);
    setSweepStatus(res.stopped ? t("stopped") : t("done"));
  } catch (e) {
    log("! sweep: " + e.message, "err");
    setSweepStatus(t("stopped"));
  } finally {
    stopSweepUI();
    // ADF back to idle when not actively sweeping
    device.adf(false).catch(() => {});
  }
});

function setSweepStatus(s) { $("sweepStatus").textContent = s; }
function stopSweepUI() {
  sweepRunning = false;
  const btn = $("sweepStart");
  if (btn) {
    btn.textContent = t("start_sweep");
    btn.className = "btn btn-success w-100 mb-2";
  }
  renderSweepPlot();
}

function commitSweepSeries(fb, fe, fs) {
  const color = SERIES_COLORS[sweepSeries.length % SERIES_COLORS.length];
  sweepSeries.push({ points: sweepLive.slice(), color, visible: true, fb, fe, fs });
  sweepLive = [];
  saveSweeps();
  renderSweepSeriesTable();
}

function renderSweepSeriesTable() {
  const tb = $("sweepSeriesTable");
  tb.innerHTML = "";
  $("noSeries").style.display = sweepSeries.length ? "none" : "block";
  sweepSeries.forEach((s, i) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td><input type="checkbox" ${s.visible ? "checked" : ""}></td>
      <td><input type="color" value="${s.color}" style="width:28px;height:24px;padding:0;border:none;background:none"></td>
      <td>${s.fs.toFixed(1)}</td>
      <td>${s.points.length}</td>
      <td><button class="btn btn-danger btn-sm" style="padding:.1rem .4rem">✕</button></td>`;
    tr.children[0].firstChild.addEventListener("change", (e) => { s.visible = e.target.checked; saveSweeps(); renderSweepPlot(); });
    tr.children[1].firstChild.addEventListener("input", (e) => { s.color = e.target.value; saveSweeps(); renderSweepPlot(); });
    tr.children[4].firstChild.addEventListener("click", () => {
      sweepSeries.splice(i, 1); saveSweeps(); renderSweepSeriesTable(); renderSweepPlot();
    });
    tb.appendChild(tr);
  });
}

$("sweepClear").addEventListener("click", () => {
  sweepSeries = []; sweepLive = [];
  saveSweeps(); renderSweepSeriesTable(); renderSweepPlot();
});
// All series side by side: f1_MHz;I1;f2_MHz;I2;… (null when there is nothing).
function sweepCsv() {
  const series = sweepSeries.map((s) => s.points).concat(sweepLive.length ? [sweepLive] : []);
  if (!series.length) return null;
  let csv = series.map((_, i) => `f${i + 1}_MHz;I${i + 1}`).join(";") + "\n";
  const maxLen = Math.max(...series.map((s) => s.length));
  for (let r = 0; r < maxLen; r++) {
    csv += series.map((s) => (s[r] ? `${s[r].x};${s[r].y}` : ";")).join(";") + "\n";
  }
  return csv;
}
$("sweepCsv").addEventListener("click", () => {
  const csv = sweepCsv();
  if (!csv) { alert(t("no_data")); return; }
  downloadCSV(csv, "sweep");
});
$("sweepPng").addEventListener("click", () => $("sweepCanvas").toBlob((b) => saveBlob(b, "sweep", "png")));
// Web Share with files (Chrome on Android/Windows/ChromeOS).
if (navigator.canShare && navigator.canShare({ files: [new File([""], "x.csv", { type: "text/csv" })] })) {
  $("sweepShare").classList.remove("d-none");
}
$("sweepShare").addEventListener("click", async () => {
  const csv = sweepCsv();
  if (!csv) { alert(t("no_data")); return; }
  const file = new File([csv], fileName("sweep", "csv"), { type: "text/csv" });
  try { await navigator.share({ files: [file], title: "ODMR sweep" }); } catch (e) { /* cancelled */ }
});
["sweepYMin", "sweepYMax"].forEach((id) => $(id).addEventListener("input", renderSweepPlot));
// "↕ Auto": back to the data's min–max, on every plot.
$("sweepFit").addEventListener("click", () => { $("sweepYMin").value = ""; $("sweepYMax").value = ""; renderSweepPlot(); });
$("liveFit").addEventListener("click", () => { $("liveYRange").value = "auto"; drawLive(); });
$("ratioFit").addEventListener("click", () => { $("ratioYRange").value = "auto"; drawRatio(); });
$("alignFit").addEventListener("click", () => { $("alignYRange").value = "auto"; drawAlign(); });

// Points and duration of the planned sweep: per point the PLL settle time plus
// one TSL2591 integration per average.
const INT_MS = [100, 200, 300, 400, 500, 600];
function updateSweepSummary() {
  const fb = parseFloat($("fBegin").value), fe = parseFloat($("fEnd").value), fs = parseFloat($("fStep").value);
  const avg = parseInt($("sweepAvg").value, 10) || 1, settle = parseInt($("sweepSettle").value, 10) || 10;
  if (!(fb < fe) || !(fs > 0)) { $("sweepSummary").textContent = ""; return; }
  const n = Math.floor((fe - fb) / fs + 1e-9) + 1;
  const sec = Math.round((n * (settle + avg * (INT_MS[parseInt($("intSelect").value, 16)] || 100))) / 1000);
  $("sweepSummary").textContent = `${n} ${t("points")} · ${t("duration")} ${Math.floor(sec / 60)}:${String(sec % 60).padStart(2, "0")} min`
    + (n > 600 ? ` · ⚠ ${t("max_points")}` : "");
}
["fBegin", "fEnd", "fStep", "sweepAvg", "sweepSettle", "intSelect"].forEach((id) => $(id).addEventListener("input", updateSweepSummary));

/* ===========================================================================
 * LIVE MONITOR TAB — fluorescence over time at one fixed frequency
 * ========================================================================= */
const livePlot = new LinePlot($("liveCanvas"), {
  xLabel: "t / s", yLabel: "I", emptyText: "—",
  xTickFmt: (v) => v.toFixed(0) + "s", yTickFmt: (v) => Math.round(v).toString(),
  pointFmt: (p) => `${p.x.toFixed(1)} s · ${Math.round(p.y)}`,
});
let liveRunning = false, liveData = [], liveStartT = 0, liveCount = 0;

$("liveStart").addEventListener("click", () => (liveRunning ? stopLive() : startLive()));
$("liveWindow").addEventListener("change", drawLive);
$("liveYRange").addEventListener("change", drawLive);
$("liveClear").addEventListener("click", () => {
  liveData = []; liveCount = 0; liveStartT = Date.now();
  $("liveCount").textContent = "0"; $("liveValue").textContent = "---";
  drawLive();
});
$("liveCsv").addEventListener("click", () => {
  if (!liveData.length) { alert(t("no_data")); return; }
  downloadCSV("t_s;f_MHz;I\n" + liveData.map((d) => `${d.t.toFixed(3)};${d.f};${d.I}`).join("\n") + "\n", "live");
});

function startLive() {
  if (liveRunning || !device.connected) return;
  liveRunning = true;
  if (!liveData.length) liveStartT = Date.now();
  $("liveStart").textContent = t("stop_live");
  $("liveStart").className = "btn btn-danger w-100 mb-3";
  liveLoop();
}
function stopLive() {
  liveRunning = false;
  $("liveStart").textContent = t("start_live");
  $("liveStart").className = "btn btn-success w-100 mb-3";
}
async function liveLoop() {
  while (liveRunning && device.connected) {
    const interval = parseInt($("liveInterval").value, 10) || 500;
    const t0 = Date.now();
    try {
      const m = await device.measure(parseFloat($("liveFreq").value));   // tunes, reads, RF off again
      liveData.push({ t: (Date.now() - liveStartT) / 1000, f: m.f, I: m.intensity });
      if (liveData.length > 5000) liveData.shift();
      liveCount++;
      $("liveValue").textContent = m.intensity;
      $("liveCount").textContent = liveCount;
      drawLive();
    } catch (e) {
      log("! live: " + e.message, "err");
    }
    await sleep(Math.max(0, interval - (Date.now() - t0)));
  }
  if (liveRunning) stopLive();
}
function drawLive() {
  const win = parseInt($("liveWindow").value, 10);
  const now = liveData.length ? liveData[liveData.length - 1].t : 0;
  const minT = Math.max(0, now - win);
  livePlot.opts.xRange = [minT, minT + win];
  livePlot.opts.yRange = $("liveYRange").value === "full" ? [0, alignRange.max] : null;
  livePlot.setSeries([{
    points: liveData.filter((d) => d.t >= minT).map((d) => ({ x: d.t, y: d.I })),
    color: "#023773", markerColor: "#85b918",
  }]);
}

/* ===========================================================================
 * RATIO / B-FIELD TAB
 * ========================================================================= */
const ratioPlot = new LinePlot($("ratioCanvas"), {
  xLabel: "t / s", yLabel: "r₁₂", zeroLine: true, emptyText: "—",
  xTickFmt: (v) => v.toFixed(0) + "s", yTickFmt: (v) => v.toFixed(2),
  pointFmt: (p) => `${p.x.toFixed(1)} s · r₁₂ ${p.y.toFixed(4)}`,
});
let ratioRunning = false, ratioData = [], ratioStart = 0, ratioCount = 0, ratioErr = 0;

$("mode3pt").addEventListener("change", updateRatioMode);
$("mode2pt").addEventListener("change", updateRatioMode);
function updateRatioMode() {
  const is3 = $("mode3pt").checked;
  $("rf3Wrap").style.display = is3 ? "block" : "none";
  $("i3Row").style.display = is3 ? "table-row" : "none";
  $("moreRatios").style.display = is3 ? "block" : "none";
}
$("ratioAvg").addEventListener("input", (e) => ($("ratioAvgVal").textContent = e.target.value));
$("ratioWindow").addEventListener("change", drawRatio);
$("ratioYRange").addEventListener("change", drawRatio);

$("ratioStart").addEventListener("click", startRatio);
$("ratioStop").addEventListener("click", stopRatio);

async function startRatio() {
  if (ratioRunning || !device.connected) return;
  ratioRunning = true;
  ratioData = []; ratioCount = 0; ratioErr = 0; ratioStart = Date.now();
  $("ratioCount").textContent = "0"; $("ratioErr").textContent = "0";
  $("ratioStart").disabled = true; $("ratioStop").disabled = false;
  $("ratioDot").classList.add("running"); $("ratioStatus").textContent = t("running");
  ratioLoop();
}
function stopRatio() {
  ratioRunning = false;
  $("ratioStart").disabled = !device.connected; $("ratioStop").disabled = true;
  $("ratioDot").classList.remove("running");
  $("ratioStatus").textContent = t("stopped");
  // ADF back to idle when ratio loop ends
  if (device.connected) device.adf(false).catch(() => {});
}

async function ratioLoop() {
  while (ratioRunning && device.connected) {
    const f1 = parseFloat($("rf1").value), f2 = parseFloat($("rf2").value);
    const is3 = $("mode3pt").checked, f3 = is3 ? parseFloat($("rf3").value) : 0;
    const avg = parseInt($("ratioAvg").value, 10);
    const interval = Math.max(200, parseInt($("ratioInterval").value, 10) || 500);
    const tStart = Date.now();
    try {
      const d = await device.ratio(f1, f2, f3, avg);
      processRatio(d);
      ratioCount++; $("ratioCount").textContent = ratioCount;
      $("ratioDot").classList.remove("error");
      if (ratioRunning) $("ratioStatus").textContent = t("running");   // a reply can land after Stop
    } catch (e) {
      ratioErr++; $("ratioErr").textContent = ratioErr;
      $("ratioDot").classList.add("error");
      log("! ratio: " + e.message, "err");
    }
    const elapsed = Date.now() - tStart;
    await sleep(Math.max(0, interval - elapsed));
  }
}

function processRatio(d) {
  const p = d.points;
  const time = (Date.now() - ratioStart) / 1000;
  const rec = {
    time, r12: d.r12, r13: d.r13 || 0, r23: d.r23 || 0,
    I1: p[0].I, I2: p[1].I, I3: p[2] ? p[2].I : null,
    f1: p[0].f, f2: p[1].f, f3: p[2] ? p[2].f : null,
  };
  ratioData.push(rec);
  if (ratioData.length > 1000) ratioData.shift();

  const disp = $("ratioDisplay");
  disp.textContent = d.r12.toFixed(4);
  disp.classList.remove("positive", "negative");
  if (d.r12 > 0.005) disp.classList.add("positive");
  else if (d.r12 < -0.005) disp.classList.add("negative");

  $("rf1Disp").textContent = rec.f1.toFixed(1);
  $("rf2Disp").textContent = rec.f2.toFixed(1);
  $("i1Disp").textContent = rec.I1; $("i2Disp").textContent = rec.I2;
  if (rec.I3 !== null) {
    $("rf3Disp").textContent = rec.f3.toFixed(1);
    $("i3Disp").textContent = rec.I3;
    $("r13Disp").textContent = rec.r13.toFixed(4);
    $("r23Disp").textContent = rec.r23.toFixed(4);
  }
  drawRatio();
}

function drawRatio() {
  const win = parseInt($("ratioWindow").value, 10);
  const now = ratioData.length ? ratioData[ratioData.length - 1].time : 0;
  const minT = Math.max(0, now - win);
  const pts = ratioData.filter((d) => d.time >= minT).map((d) => ({ x: d.time, y: d.r12 }));
  const yr = $("ratioYRange").value;
  ratioPlot.opts.xRange = [minT, minT + win];
  ratioPlot.opts.yRange = yr === "auto" ? null : [-parseFloat(yr), parseFloat(yr)];
  ratioPlot.setSeries([{ points: pts, color: "#023773", markerColor: "#85b918" }]);
}

$("ratioClear").addEventListener("click", () => {
  ratioData = []; ratioCount = 0; ratioErr = 0; ratioStart = Date.now();
  $("ratioCount").textContent = "0"; $("ratioErr").textContent = "0";
  $("ratioDisplay").textContent = "---";
  drawRatio();
});
$("ratioCsv").addEventListener("click", () => {
  if (!ratioData.length) { alert(t("no_data")); return; }
  const has3 = ratioData[0].f3 !== null;
  let csv = "t_s;f1;I1;f2;I2;r12" + (has3 ? ";f3;I3;r13;r23" : "") + "\n";
  for (const d of ratioData) {
    csv += `${d.time.toFixed(3)};${d.f1};${d.I1};${d.f2};${d.I2};${d.r12.toFixed(6)}`;
    if (has3) csv += `;${d.f3};${d.I3};${d.r13.toFixed(6)};${d.r23.toFixed(6)}`;
    csv += "\n";
  }
  downloadCSV(csv, "ratio");
});

/* ===========================================================================
 * JUSTAGE / ALIGNMENT TAB
 * ========================================================================= */
let alignRunning = false, alignData = [], alignStartT = 0;

// Highest count the TSL2591 reaches per gain_integration setting (measured, as on
// the device's justage page); "optimal" is 80 % of it, above 88 % it saturates.
const EST_MAX = {
  "0x00_0x00": 879, "0x00_0x01": 1736, "0x00_0x02": 2593, "0x00_0x03": 3451, "0x00_0x04": 4309, "0x00_0x05": 5168,
  "0x10_0x00": 37888, "0x10_0x01": 65535, "0x10_0x02": 65535, "0x10_0x03": 65535, "0x10_0x04": 65535, "0x10_0x05": 65535,
  "0x20_0x00": 37888, "0x20_0x01": 65535, "0x20_0x02": 65535, "0x20_0x03": 65535, "0x20_0x04": 65535, "0x20_0x05": 65535,
  "0x30_0x00": 37888, "0x30_0x01": 65535, "0x30_0x02": 65535, "0x30_0x03": 65535, "0x30_0x04": 65535, "0x30_0x05": 65535,
};
let alignRange = { max: 37888, opt: 30310, good: 11366, saturated: 33341 };
function updateAlignRange() {
  const max = EST_MAX[$("gainSelect").value + "_" + $("intSelect").value] || 65535;
  const opt = Math.max(100, Math.round(max * 0.8));
  alignRange = { max, opt, good: Math.round(opt * 0.375), saturated: Math.round(opt * 1.1) };
  $("alignMax").textContent = max;
  $("liveMax").textContent = max;
  $("alignOptimal").textContent = `${opt} – ${alignRange.saturated}`;
  drawAlign();
  drawLive();
}
$("gainSelect").addEventListener("change", updateAlignRange);
$("intSelect").addEventListener("change", updateAlignRange);

const alignPlot = new LinePlot($("alignCanvas"), {
  xLabel: "t / s", yLabel: "I", emptyText: "—",
  xTickFmt: (v) => v.toFixed(0) + "s", yTickFmt: (v) => Math.round(v).toString(),
  pointFmt: (p) => `${p.x.toFixed(1)} s · ${Math.round(p.y)}`,
});
function drawAlign() {
  const win = parseInt($("alignWindow").value, 10);
  const now = alignData.length ? alignData[alignData.length - 1].x : 0;
  const minT = Math.max(0, now - win);
  const pts = alignData.filter((d) => d.x >= minT);
  const ys = pts.map((p) => p.y), lo = Math.min(...ys), hi = Math.max(...ys);
  const mode = $("alignYRange").value;
  if (mode === "full") alignPlot.opts.yRange = [0, alignRange.max];
  else if (mode === "stretch" && ys.length) alignPlot.opts.yRange = [lo, hi];
  else if (ys.length > 1) {
    const m = Math.max(50, (hi - lo) * 0.1);
    alignPlot.opts.yRange = [Math.max(0, lo - m), hi + m];
  } else alignPlot.opts.yRange = null;
  alignPlot.opts.xRange = [minT, minT + win];
  alignPlot.setSeries([{ points: pts, color: "#023773", markerColor: "#85b918" }]);
}
$("alignWindow").addEventListener("change", drawAlign);
$("alignYRange").addEventListener("change", drawAlign);
$("alignClear").addEventListener("click", () => { alignData = []; alignStartT = Date.now(); drawAlign(); });

$("alignStart").addEventListener("click", startAlign);
$("alignStop").addEventListener("click", stopAlign);
$("applyTsl").addEventListener("click", applyTsl);

async function startAlign() {
  if (alignRunning || !device.connected) return;
  alignRunning = true;
  if (!alignData.length) alignStartT = Date.now();
  $("alignStart").disabled = true; $("alignStop").disabled = false;
  $("alignStatus").textContent = t("monitoring"); $("alignStatus").className = "badge bg-info";
  $("guideLiveBtn").textContent = t("stop_align"); $("guideLiveBtn").className = "btn btn-danger btn-sm ms-auto";
  alignLoop();
}
function stopAlign() {
  alignRunning = false;
  $("alignStart").disabled = !device.connected; $("alignStop").disabled = true;
  $("guideLiveBtn").textContent = t("start_align"); $("guideLiveBtn").className = "btn btn-success btn-sm ms-auto";
  if ($("alignStatus").className.includes("bg-info")) {
    $("alignStatus").textContent = t("ready"); $("alignStatus").className = "badge bg-secondary";
  }
}
async function alignLoop() {
  while (alignRunning && device.connected) {
    try {
      const v = await device.intensity();
      const r = alignRange;
      const pct = Math.min((v / r.max) * 100, 100);
      const [key, cls, color] = v > r.saturated ? ["saturated", "bg-danger", "--uc2-danger"]
        : v > r.opt ? ["optimal", "bg-success", "--uc2-green"]
        : v > r.good ? ["good", "bg-warning", "--uc2-warning"] : ["adjust", "bg-danger", "--uc2-danger"];
      // Same readout on the alignment tab and in the guide
      for (const [val, bar, st] of [["alignValue", "alignBar", "alignStatus"], ["guideValue", "guideBar", "guideStatus"]]) {
        $(val).textContent = v;
        $(bar).style.width = pct.toFixed(1) + "%";
        $(bar).style.background = `var(${color})`;
        $(st).textContent = t(key);
        $(st).className = "badge " + cls;
      }
      $("alignBar").textContent = pct.toFixed(1) + "%";
      alignData.push({ x: (Date.now() - alignStartT) / 1000, y: v });
      if (alignData.length > 5000) alignData.shift();
      drawAlign();
    } catch (e) {
      log("! intensity: " + e.message, "err");
    }
    await sleep(400);
  }
}
async function applyTsl() {
  if (!device.connected) return;
  const st = $("tslStatus");
  st.textContent = t("applying"); st.className = "mt-2 small text-info";
  try {
    await device.setGain($("gainSelect").value);
    await device.setIntegration(parseInt($("intSelect").value, 16));
    st.textContent = t("applied"); st.className = "mt-2 small text-success";
    updateAlignRange();
    updateSweepSummary();
  } catch (e) {
    st.textContent = t("apply_err"); st.className = "mt-2 small text-danger";
    log("! tsl: " + e.message, "err");
  }
}

/* ===========================================================================
 * GUIDE TAB — steps and scenes in guide.js
 * ========================================================================= */
let guideStep = 0;
try { guideStep = Math.min(SCENES.length - 1, parseInt(localStorage.getItem("odmr_guide_step"), 10) || 0); } catch (e) { /* blocked */ }

function renderGuide() {
  const steps = STEPS[lang] || STEPS.de;
  const [title, text, checks] = steps[guideStep];
  const scene = SCENES[guideStep];
  $("guideTitle").textContent = `${guideStep + 1}. ${title}`;
  $("guideText").textContent = text;
  $("guideProgress").textContent = `${t("step")} ${guideStep + 1} / ${steps.length}`;
  $("guideChecks").replaceChildren(...checks.map((c) => {
    const li = document.createElement("li"), label = document.createElement("label"), box = document.createElement("input");
    box.type = "checkbox";
    label.append(box, c);
    li.append(label);
    return li;
  }));
  $("guideLive").classList.toggle("d-none", !scene.live);
  $("guideGoto").classList.toggle("d-none", !scene.goto);
  if (scene.goto) $("guideGoto").textContent = t("goto_" + scene.goto.replace("-", "_"));
  $("guidePrev").disabled = guideStep === 0;
  $("guideNext").disabled = guideStep === steps.length - 1;
  $("guideList").replaceChildren(...steps.map(([stepTitle], i) => {
    const li = document.createElement("li");
    li.textContent = stepTitle;
    li.classList.toggle("active", i === guideStep);
    li.addEventListener("click", () => showGuideStep(i));
    return li;
  }));
  // Scene: which parts show, which are highlighted, which animate
  const has = (list, id) => (list || "").split(" ").includes(id);
  for (const el of $("guideScene").querySelectorAll("[id^='g-']")) {
    const id = el.id.slice(2);
    el.classList.toggle("hidden", !has(scene.show, id));
    el.classList.toggle("focus", has(scene.focus, id));
    el.classList.toggle("run", has(scene.run, id));
    el.classList.remove("draw");
  }
  void $("guideScene").getBoundingClientRect();   // restart the draw animation
  for (const id of (scene.draw || "").split(" ").filter(Boolean)) $("g-" + id).classList.add("draw");
}
function showGuideStep(i) {
  guideStep = Math.max(0, Math.min(SCENES.length - 1, i));
  try { localStorage.setItem("odmr_guide_step", guideStep); } catch (e) { /* blocked */ }
  renderGuide();
}
$("guidePrev").addEventListener("click", () => showGuideStep(guideStep - 1));
$("guideNext").addEventListener("click", () => showGuideStep(guideStep + 1));
$("guideLiveBtn").addEventListener("click", () => (alignRunning ? stopAlign() : startAlign()));
$("guideGoto").addEventListener("click", () => showTab(SCENES[guideStep].goto));

/* ---------------------------------------------------------------------------
 * Tabs: #tab-… in the URL opens a tab, else the one used last
 * ------------------------------------------------------------------------- */
function showTab(id) {
  const btn = document.querySelector(`[data-bs-target="#${id}"]`);
  if (btn) bootstrap.Tab.getOrCreateInstance(btn).show();
}
document.addEventListener("shown.bs.tab", (e) => {
  const id = e.target.dataset.bsTarget.slice(1);
  history.replaceState(null, "", location.pathname + location.search + "#" + id);
  try { localStorage.setItem("odmr_tab", id); } catch (err) { /* blocked */ }
});
document.querySelectorAll("[data-goto]").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.goto)));
window.addEventListener("hashchange", () => showTab(location.hash.slice(1)));

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function fileName(prefix, ext) {
  return `${prefix}_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.${ext}`;
}
function saveBlob(blob, prefix, ext) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = fileName(prefix, ext);
  a.click();
  // Revoke later, not at once: the download may read the blob after click() returns.
  setTimeout(() => URL.revokeObjectURL(a.href), 60000);
}
function downloadCSV(text, prefix) {
  saveBlob(new Blob([text], { type: "text/csv;charset=utf-8;" }), prefix, "csv");
}

function refreshDynamicLabels() {
  // keep button labels consistent with running state after a language switch
  if (!sweepRunning) { const b = $("sweepStart"); if (b) b.textContent = t("start_sweep"); }
  else { const b = $("sweepStart"); if (b) b.textContent = t("stop_sweep"); }
  $("liveStart").textContent = liveRunning ? t("stop_live") : t("start_live");
  $("guideLiveBtn").textContent = alignRunning ? t("stop_align") : t("start_align");
  $("modeBadge").textContent = t(!ON_BOARD ? "mode_serial" : VIA_USB_NET ? "mode_usb" : "mode_wifi");
  renderGuide();
  updateSweepSummary();
  $("btnToggleLog").textContent = $("logWrap").style.display === "none" ? t("show_log") : t("hide_log");
  $("connText").textContent = deviceConnected ? t("connected") : t("not_connected");
  showWifi(wifiState);
}

/* ---------------------------------------------------------------------------
 * Boot
 * ------------------------------------------------------------------------- */
(function boot() {
  loadSweeps();
  updateLanguage();
  updateRatioMode();
  updateAlignRange();
  renderSweepSeriesTable();
  renderSweepPlot();
  drawLive();
  drawRatio();
  let tab = location.hash.slice(1);
  try { tab = tab || localStorage.getItem("odmr_tab"); } catch (e) { /* blocked */ }
  if (tab) showTab(tab);

  if (ON_BOARD) {
    document.body.classList.add("on-board");
    device.connect().catch((e) => log("! connect: " + e.message, "err"));
    return;
  }
  if (!SerialDevice.supported) {
    document.body.classList.add("no-serial");
    return;
  }
  // Try to silently re-open a previously granted port.
  device.tryReconnect().then((ok) => {
    if (ok) log("✓ reconnected to known port", "muted");
  });
  // React to physical plug/unplug.
  navigator.serial.addEventListener("disconnect", () => {
    if (device.port === null) log("✗ device disconnected", "err");
  });
})();
