# ODMR Serial Command Protocol

The ESP32-S3/C3 ODMR firmware exposes a line-based ASCII protocol over USB-CDC
serial (115200 baud) that mirrors the full HTTP/web API. This lets the device be
driven entirely over serial — e.g. by the standalone
[`webserial-app/`](webserial-app/) — with no WiFi connection.

- **Host → device:** one command per line, terminated by `\n` (or `\r\n`).
  The command keyword is **case-insensitive**; parameters are space-separated.
- **Device → host:** one or more response lines, each beginning with a tag
  (`PONG`, `DATA`, `INT`, `RATIO`, `SWEEP`, `TSL`, `OK`, `ERR`, …). Structured
  payloads are emitted as JSON after the tag.
- Implemented in `processSerialLine()` / `serialSweep()` in `src/main.cpp`.
- Parsed by `SerialDevice` in `webserial-app/assets/serial.js`.

Frequencies are in **MHz** and must lie within `[2200, 4400]` (the ADF4351
range). Intensity is the TSL2591 IR channel (0…65535).

---

## Commands

### `PING`
Connectivity check.
```
> PING
< PONG
```

### `VERSION`
Firmware version info.
```
> VERSION
< VERSION {"version":"1.0.0","build_date":"2026-06-10","build_time":"08:57:03","git_hash":"ce85a7c","git_branch":"main"}
```

### `STATUS`
Runtime status.
```
> STATUS
< STATUS {"clients":0,"fmin":2200.0,"fmax":4400.0,"led":1,"sweep":false,"tsl":true,"gain":32,"integration_time":0}
```

### `MEASURE <f>`
Tune to `f` MHz and read intensity. `bfield` is a reserved placeholder (`0.0`).
A frequency outside the valid range returns an intensity-only read (use `0` for
a plain live read at the current frequency).
```
> MEASURE 2870
< DATA 2870.0 12345 0.0
```

### `INTENSITY`
Fast, cached photodiode read (does not retune). Used for alignment.
```
> INTENSITY
< INT 4099
```

### `RATIO <f1> <f2> <f3|0> <avg>`
Measure intensities at the given frequencies and compute normalised ratios
`r = (Iᵢ - Iⱼ)/(Iᵢ + Iⱼ)`. Pass `f3 = 0` for 2-point mode. `avg` = 1…20.
```
> RATIO 2865 2875 0 3
< RATIO {"avg":3,"points":[{"f":2865.0,"I":1200},{"f":2875.0,"I":1000}],"r12":0.090909,"r13":0.000000,"r23":0.000000}

> RATIO 2865 2875 2870 5
< RATIO {"avg":5,"points":[{"f":2865.0,"I":1200},{"f":2875.0,"I":1000},{"f":2870.0,"I":800}],"r12":0.090909,"r13":0.200000,"r23":0.111111}
```

### `SWEEP <f_begin> <f_end> <f_step> [avg] [settle]`
Stream a frequency sweep. `avg` (default 1) = readings averaged per point;
`settle` (default 10) = PLL settle time in ms. Emits a `START` line, one `DATA`
line per point, then `DONE` (or `STOP` if interrupted). Max 600 points.
```
> SWEEP 2820 2920 2 1 10
< SWEEP START {"f_begin":2820.0,"f_end":2920.0,"f_step":2.0,"avg":1,"settle":10,"total":51}
< SWEEP DATA 0 51 2820.0 19850
< SWEEP DATA 1 51 2822.0 19790
  …
< SWEEP DONE 51
```

### `SWEEPSTOP`
Abort a running sweep. The firmware checks for this between points.
```
> SWEEPSTOP
< SWEEP STOP 23        (emitted by the in-progress sweep)
```

### `GAIN <value>`
Set TSL2591 gain. `value` is the register encoding (`0x00` low, `0x10` medium,
`0x20` high, `0x30` max); decimal `32` or hex `0x20` both accepted.
```
> GAIN 0x20
< OK GAIN 0x20
```

### `INTTIME <0..5>`
Set TSL2591 integration time (`0`=100 ms … `5`=600 ms).
```
> INTTIME 0
< OK INTTIME 0
```

### `GETTSL`
Read current TSL2591 settings.
```
> GETTSL
< TSL {"gain":32,"integration_time":0}
```

### `ADFON` / `ADFOFF`
Enable / disable the ADF4351 RF output.
```
> ADFOFF
< OK ADF OFF
```

### `HELP`
List available commands.
```
> HELP
< CMDS PING VERSION STATUS MEASURE INTENSITY RATIO SWEEP SWEEPSTOP GAIN INTTIME GETTSL ADFON ADFOFF
```

---

## Errors

Unrecognised commands and bad parameters return an `ERR …` line, e.g.:
```
< ERR unknown command: FOO
< SWEEP ERR {"error":"invalid sweep parameters"}
< RATIO ERR {"error":"need f1 f2"}
```

---

## Notes for clients

- Send one command at a time and wait for its terminating response before
  sending the next — responses are line-based and not tagged with a request id.
  (`SerialDevice` enforces this with an internal command queue.)
- A sweep is a single long-running streamed response; `SWEEPSTOP` may be sent
  while it runs (the firmware reads it mid-sweep).
- The `DATA` line carries three fields (`f intensity bfield`) for backward
  compatibility with the original `messung_webserial.html` parser.
