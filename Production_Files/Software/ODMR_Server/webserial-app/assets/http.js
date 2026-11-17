/* ===========================================================================
 * HttpDevice — the same interface as SerialDevice (serial.js), but talking to
 * the board's HTTP API. Used when the board serves this app itself:
 * http://192.168.4.1/app/ over WiFi, http://192.168.7.1/app/ over the USB
 * cable (iPad, iPhone). Endpoints: see src/main.cpp.
 * =========================================================================== */

export class HttpDevice {
  constructor() {
    this.port = null;          // SerialDevice compatibility
    this.connected = false;
    this._lineHandlers = new Set();
    this._stateHandlers = new Set();
    this._queue = Promise.resolve();
    this._stopRequested = false;
  }

  static get supported() {
    return true;
  }

  onLine(fn) {
    this._lineHandlers.add(fn);
  }

  onStateChange(fn) {
    this._stateHandlers.add(fn);
  }

  _log(line) {
    for (const h of this._lineHandlers) h(line);
  }

  _emitState() {
    for (const h of this._stateHandlers) h(this.connected);
  }

  // One request at a time, like the serial command queue: the board measures
  // in its request handlers.
  _enqueue(task) {
    const run = this._queue.then(task, task);
    this._queue = run.catch(() => {});
    return run;
  }

  _request(method, path, params = {}, timeoutMs = 6000) {
    return this._enqueue(async () => {
      const query = new URLSearchParams(params).toString();
      const url = method === "GET" && query ? `${path}?${query}` : path;
      const abort = new AbortController();
      const timer = setTimeout(() => abort.abort(), timeoutMs);
      try {
        const res = await fetch(url, {
          method,
          cache: "no-store",
          signal: abort.signal,
          body: method === "POST" ? new URLSearchParams(params) : undefined,
        });
        const text = (await res.text()).trim();
        this._log(`${res.status} ${method} ${url} ${text}`.slice(0, 200));
        if (!res.ok) throw new Error(text || `HTTP ${res.status}`);
        return text;
      } catch (e) {
        throw new Error(e.name === "AbortError" ? `Timeout (${path})` : e.message);
      } finally {
        clearTimeout(timer);
      }
    });
  }

  async connect() {
    await this.getVersion();
    this.connected = true;
    this._emitState();
  }

  async tryReconnect() {
    try {
      await this.connect();
      return true;
    } catch (e) {
      return false;
    }
  }

  async disconnect() {
    this.connected = false;
    this._emitState();
  }

  async writeRaw() {
    throw new Error("Manual commands need USB serial (Web Serial); not available from the board's own page.");
  }

  async getVersion() {
    return JSON.parse(await this._request("GET", "/version"));
  }

  async getTSL() {
    return JSON.parse(await this._request("GET", "/tsl/settings"));
  }

  // Tune to freq (MHz) and read intensity. Returns {f, intensity, bfield}.
  async measure(freqMHz) {
    const p = (await this._request("GET", "/measure", { frequenz: freqMHz })).split(/\s+/);
    return { f: parseFloat(p[0]), intensity: parseInt(p[1], 10), bfield: parseFloat(p[2]) || 0 };
  }

  async intensity() {
    return JSON.parse(await this._request("GET", "/intensity", {}, 2500)).intensity;
  }

  // 2- or 3-point ratio, f3 = 0 -> 2-point. Same shape as the serial RATIO reply.
  async ratio(f1, f2, f3, avg) {
    const params = { f1, f2, avg: avg || 3 };
    if (f3) params.f3 = f3;
    const d = JSON.parse(await this._request("GET", "/ratio", params));
    return { avg: d.avg, points: d.points, ...d.ratio };
  }

  async setGain(gainHex) {
    return this._request("POST", "/tsl/gain", { gain: parseInt(gainHex, 16) });
  }

  async setIntegration(value) {
    return this._request("POST", "/tsl/integration_time", { integration_time: value });
  }

  async adf(on) {
    return this._request("POST", on ? "/ADF_Enable" : "/ADF_Disable");
  }

  // Streams over Server-Sent Events: {"f","I","idx","total"} per point, then {"done":true}.
  sweep(fBegin, fEnd, fStep, avg, settle, { onStart, onPoint } = {}) {
    return this._enqueue(
      () =>
        new Promise((resolve, reject) => {
          this._stopRequested = false;
          const query = new URLSearchParams({ f_begin: fBegin, f_end: fEnd, f_step: fStep, avg: avg || 1, settle: settle || 10 });
          const source = new EventSource(`/sweep?${query}`);
          let count = 0, timer;
          const finish = (fn) => {
            clearTimeout(timer);
            source.close();
            fn();
          };
          const resetTimer = () => {
            clearTimeout(timer);
            timer = setTimeout(() => finish(() => reject(new Error("Sweep timed out (no data)"))), 8000);
          };
          resetTimer();
          source.onmessage = (e) => {
            resetTimer();
            const d = JSON.parse(e.data);
            if (d.done) {
              finish(() => resolve({ stopped: this._stopRequested, count }));
              return;
            }
            if (count === 0 && onStart) onStart({ total: d.total });
            count++;
            if (onPoint) onPoint(d.idx, d.total, d.f, d.I);
          };
          source.onerror = () => {
            if (this._stopRequested) finish(() => resolve({ stopped: true, count }));
            else finish(() => reject(new Error("Sweep rejected or connection lost")));
          };
        })
    );
  }

  // Written directly: the sweep holds the queue.
  async stopSweep() {
    this._stopRequested = true;
    await fetch("/sweep_stop", { method: "POST" }).catch(() => {});
  }
}
