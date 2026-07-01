/* ===========================================================================
 * openUC2 ODMR — WebSerial app controller
 * Wires the tabbed UI (sweep, ratio/B-field, alignment) to the SerialDevice.
 * =========================================================================== */
import { SerialDevice } from "./serial.js";
import { LinePlot } from "./plot.js";

const $ = (id) => document.getElementById(id);
const device = new SerialDevice();

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
    align_hint: "Optischen Pfad für maximale Intensität justieren.", align_guide: "Justage-Anleitung",
    align_s1: "Auf „Start Überwachung\" klicken.", align_s2: "Die Board-LED leuchtet blau (Intensitätsmodus aktiv).",
    align_s3: "Optische Komponenten (Laser, Linsen, Spiegel) justieren.", align_s4: "Intensitätswerte beobachten und maximieren.",
    align_s5: "Nach Abschluss „Stop\" klicken.", range: "Messbereich", sensor_settings: "Sensor-Einstellungen",
    gain: "Verstärkung (Gain)", integration: "Integrationszeit", apply: "Einstellungen anwenden",
    optimal: "Optimal", good: "Gut", adjust: "Justage erforderlich", monitoring: "Überwachung aktiv",
    applied: "Einstellungen angewendet", applying: "Wird angewendet…", apply_err: "Fehler beim Anwenden",
    info_title: "Über dieses WebSerial-Interface", protocol_title: "Serielles Protokoll",
    col_cmd: "Befehl", col_resp: "Antwort", no_serial_title: "Web Serial wird nicht unterstützt",
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
    align_hint: "Adjust the optical path for maximum intensity.", align_guide: "Alignment Guide",
    align_s1: "Click \"Start monitoring\".", align_s2: "The board LED turns blue (intensity mode active).",
    align_s3: "Adjust optics (laser, lenses, mirrors).", align_s4: "Watch the intensity and maximise it.",
    align_s5: "Click \"Stop\" when done.", range: "Measurement Range", sensor_settings: "Sensor Settings",
    gain: "Gain", integration: "Integration time", apply: "Apply settings",
    optimal: "Optimal", good: "Good", adjust: "Adjustment required", monitoring: "Monitoring active",
    applied: "Settings applied", applying: "Applying…", apply_err: "Failed to apply",
    info_title: "About this WebSerial interface", protocol_title: "Serial Protocol",
    col_cmd: "Command", col_resp: "Response", no_serial_title: "Web Serial not supported",
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
  $("ratioStart").disabled = !on;
  $("alignStart").disabled = !on;
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
    } catch (e) { /* sensor may be absent */ }
  } else {
    dot.className = "conn-dot";
    txt.textContent = t("not_connected");
    $("deviceVersion").textContent = "";
    setControlsEnabled(false);
    stopSweepUI(); stopRatio(); stopAlign();
  }
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
});
let sweepLive = [];           // current sweep points {x,y}
let sweepSeries = [];         // [{points, color, visible, fb, fe, fs}]
let sweepRunning = false;
const SERIES_COLORS = ["#023773", "#dc3545", "#1f9c7c", "#e08e0b", "#7048b6", "#0a8a4a"];

function renderSweepPlot() {
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
  if (!(fb < fe) || fs <= 0) { alert("f_begin < f_end und Schrittweite > 0 erforderlich."); return; }

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
    tr.children[0].firstChild.addEventListener("change", (e) => { s.visible = e.target.checked; renderSweepPlot(); });
    tr.children[1].firstChild.addEventListener("input", (e) => { s.color = e.target.value; renderSweepPlot(); });
    tr.children[4].firstChild.addEventListener("click", () => {
      sweepSeries.splice(i, 1); renderSweepSeriesTable(); renderSweepPlot();
    });
    tb.appendChild(tr);
  });
}

$("sweepClear").addEventListener("click", () => {
  sweepSeries = []; sweepLive = [];
  renderSweepSeriesTable(); renderSweepPlot();
});
$("sweepCsv").addEventListener("click", () => {
  if (!sweepSeries.length && !sweepLive.length) { alert("Keine Daten."); return; }
  const series = sweepSeries.map((s) => s.points).concat(sweepLive.length ? [sweepLive] : []);
  let header = series.map((_, i) => `f${i + 1}_MHz;I${i + 1}`).join(";");
  const maxLen = Math.max(...series.map((s) => s.length));
  let csv = header + "\n";
  for (let r = 0; r < maxLen; r++) {
    csv += series.map((s) => (s[r] ? `${s[r].x};${s[r].y}` : ";")).join(";") + "\n";
  }
  downloadCSV(csv, "sweep");
});

/* ===========================================================================
 * RATIO / B-FIELD TAB
 * ========================================================================= */
const ratioPlot = new LinePlot($("ratioCanvas"), {
  xLabel: "t / s", yLabel: "r₁₂", zeroLine: true, emptyText: "—",
  xTickFmt: (v) => v.toFixed(0) + "s", yTickFmt: (v) => v.toFixed(2),
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
      $("ratioDot").classList.remove("error"); $("ratioStatus").textContent = t("running");
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
  ratioPlot.opts.xRange = [minT, minT + win];
  ratioPlot.setSeries([{ points: pts, color: "#023773", markerColor: "#85b918" }]);
}

$("ratioClear").addEventListener("click", () => {
  ratioData = []; ratioCount = 0; ratioErr = 0; ratioStart = Date.now();
  $("ratioCount").textContent = "0"; $("ratioErr").textContent = "0";
  $("ratioDisplay").textContent = "---";
  drawRatio();
});
$("ratioCsv").addEventListener("click", () => {
  if (!ratioData.length) { alert("Keine Daten."); return; }
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
let alignRunning = false;
$("alignStart").addEventListener("click", startAlign);
$("alignStop").addEventListener("click", stopAlign);
$("applyTsl").addEventListener("click", applyTsl);

async function startAlign() {
  if (alignRunning || !device.connected) return;
  alignRunning = true;
  $("alignStart").disabled = true; $("alignStop").disabled = false;
  $("alignStatus").textContent = t("monitoring"); $("alignStatus").className = "badge bg-info";
  alignLoop();
}
function stopAlign() {
  alignRunning = false;
  $("alignStart").disabled = !device.connected; $("alignStop").disabled = true;
  if ($("alignStatus").className.includes("bg-info")) {
    $("alignStatus").textContent = t("ready"); $("alignStatus").className = "badge bg-secondary";
  }
}
async function alignLoop() {
  while (alignRunning && device.connected) {
    try {
      const v = await device.intensity();
      $("alignValue").textContent = v;
      const pct = Math.min((v / 65535) * 100, 100);
      const bar = $("alignBar");
      bar.style.width = pct.toFixed(1) + "%";
      bar.textContent = pct.toFixed(1) + "%";
      const st = $("alignStatus");
      if (v > 4000) { st.textContent = t("optimal"); st.className = "badge bg-success"; bar.style.background = "var(--uc2-green)"; }
      else if (v > 1500) { st.textContent = t("good"); st.className = "badge bg-warning"; bar.style.background = "var(--uc2-warning)"; }
      else { st.textContent = t("adjust"); st.className = "badge bg-danger"; bar.style.background = "var(--uc2-danger)"; }
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
  } catch (e) {
    st.textContent = t("apply_err"); st.className = "mt-2 small text-danger";
    log("! tsl: " + e.message, "err");
  }
}

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function downloadCSV(text, prefix) {
  const blob = new Blob([text], { type: "text/csv;charset=utf-8;" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `${prefix}_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.csv`;
  a.click();
  URL.revokeObjectURL(a.href);
}

function refreshDynamicLabels() {
  // keep button labels consistent with running state after a language switch
  if (!sweepRunning) { const b = $("sweepStart"); if (b) b.textContent = t("start_sweep"); }
  else { const b = $("sweepStart"); if (b) b.textContent = t("stop_sweep"); }
  $("btnToggleLog").textContent = $("logWrap").style.display === "none" ? t("show_log") : t("hide_log");
  $("connText").textContent = deviceConnected ? t("connected") : t("not_connected");
}

/* ---------------------------------------------------------------------------
 * Boot
 * ------------------------------------------------------------------------- */
(function boot() {
  updateLanguage();
  updateRatioMode();
  renderSweepSeriesTable();
  renderSweepPlot();
  drawRatio();

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
