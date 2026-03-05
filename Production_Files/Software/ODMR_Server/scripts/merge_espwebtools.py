import csv
import subprocess
from pathlib import Path

Import("env")


def _parse_spiffs_offset(partitions_csv: Path) -> str:
    with partitions_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.reader(f):
            if not row or row[0].strip().startswith("#"):
                continue
            if len(row) < 5:
                continue
            name = row[0].strip().lower()
            p_type = row[1].strip().lower()
            subtype = row[2].strip().lower()
            offset = row[3].strip().lower()

            if name == "spiffs" or (p_type == "data" and subtype == "spiffs"):
                if not offset:
                    raise RuntimeError(f"SPIFFS offset empty in {partitions_csv}")
                return offset
    raise RuntimeError(f"No SPIFFS partition found in {partitions_csv}")


def _find_boot_app0(env) -> Path:
    build_dir = Path(env.subst("$BUILD_DIR"))
    local = build_dir / "boot_app0.bin"
    if local.exists():
        return local

    p = env.PioPlatform()
    fw = p.get_package_dir("framework-arduinoespressif32")
    if fw:
        cand = Path(fw) / "tools" / "partitions" / "boot_app0.bin"
        if cand.exists():
            return cand

    pkg_dir = Path(env.subst("$PIOPACKAGES_DIR"))
    hits = list(pkg_dir.rglob("boot_app0.bin"))
    if hits:
        return hits[0]

    raise RuntimeError("boot_app0.bin not found")


def _find_app_bin(env) -> Path:
    build_dir = Path(env.subst("$BUILD_DIR"))
    progname = env.subst("$PROGNAME")
    cand = build_dir / f"{progname}.bin"
    if cand.exists():
        return cand

    cand2 = build_dir / "firmware.bin"
    if cand2.exists():
        return cand2

    bins = [p for p in build_dir.glob("*.bin") if p.name not in ("bootloader.bin", "partitions.bin", "spiffs.bin", "boot_app0.bin")]
    if bins:
        bins.sort(key=lambda p: p.stat().st_size, reverse=True)
        return bins[0]

    raise RuntimeError("Application .bin not found (run: pio run -t buildprog)")


def _bootloader_addr(chip: str) -> str:
    chip = (chip or "").lower()
    if chip in ("esp32", "esp32p4"):
        return "0x1000"
    return "0x0"


def _get_flash_freq(env) -> str:
    """Read flash frequency from board config and convert to esptool format string."""
    try:
        f_flash = int(env.BoardConfig().get("build.f_flash", 80000000))
    except (KeyError, ValueError, TypeError):
        f_flash = 80000000
    freq_map = {
        80000000: "80m",
        40000000: "40m",
        26000000: "26m",
        20000000: "20m",
        26213000: "26m",  # handle slight rounding
    }
    result = freq_map.get(f_flash)
    if result is None:
        # Fallback: round to nearest supported
        result = "80m" if f_flash >= 60000000 else "40m"
    return result


def _merge_bins(target, source, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    proj_dir = Path(env.subst("$PROJECT_DIR"))
    pioenv = env.subst("$PIOENV")

    chip = env.BoardConfig().get("build.mcu", "esp32")
    flash_mode = env.BoardConfig().get("build.flash_mode", "dio")
    flash_size = env.GetProjectOption("board_upload.flash_size") or "4MB"
    flash_freq = _get_flash_freq(env)
    print(f"[mergedbin] chip={chip} flash_mode={flash_mode} flash_size={flash_size} flash_freq={flash_freq}")

    partitions_name = env.BoardConfig().get("build.partitions")
    partitions_csv = Path(partitions_name)
    if not partitions_csv.is_absolute():
        partitions_csv = proj_dir / partitions_name

    spiffs_offset = _parse_spiffs_offset(partitions_csv)

    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    boot_app0 = _find_boot_app0(env)
    app_bin = _find_app_bin(env)
    spiffs = build_dir / "spiffs.bin"

    missing = [p for p in (bootloader, partitions, spiffs) if not p.exists()]
    if missing:
        raise RuntimeError(
            "Missing: " + ", ".join(str(p) for p in missing) +
            " (run: pio run -t buildprog -t buildfs)"
        )

    out_dir = proj_dir / "build" / "fw-images"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_bin = out_dir / f"{pioenv}.bin"

    cmd = [
        env.subst("$PYTHONEXE"), "-m", "esptool",
        "--chip", chip,
        "merge-bin",
        "-o", str(out_bin),
        "--flash-mode", "keep",   # preserve DIO set by ESP-IDF bootloader for ROM-stage init
        "--flash-freq", "keep",   # preserve freq baked in by toolchain
        "--flash-size", str(flash_size),
        _bootloader_addr(chip), str(bootloader),
        "0x8000", str(partitions),
        "0xe000", str(boot_app0),
        "0x10000", str(app_bin),
        spiffs_offset, str(spiffs),
    ]

    print("Merging to:", out_bin)
    print(" ".join(cmd))
    subprocess.check_call(cmd, cwd=str(build_dir))


def _upload_merged(target, source, env):
    proj_dir = Path(env.subst("$PROJECT_DIR"))
    pioenv = env.subst("$PIOENV")
    chip = env.BoardConfig().get("build.mcu", "esp32")

    flash_mode = env.BoardConfig().get("build.flash_mode", "dio")
    flash_size = env.GetProjectOption("board_upload.flash_size") or "4MB"
    flash_freq = _get_flash_freq(env)

    port = env.subst("$UPLOAD_PORT")
    speed = env.subst("$UPLOAD_SPEED") or "460800"

    out_bin = proj_dir / "build" / "fw-images" / f"{pioenv}.bin"
    if not out_bin.exists():
        raise RuntimeError(f"Merged bin not found: {out_bin}")

    cmd = [
        env.subst("$PYTHONEXE"), "-m", "esptool",
        "--chip", chip,
        "-p", port,
        "-b", str(speed),
        "write-flash",
        "--flash-mode", str(flash_mode),
        "--flash-freq", flash_freq,
        "--flash-size", str(flash_size),
        "0x0", str(out_bin),
    ]

    print("Flashing merged:", out_bin)
    print(" ".join(cmd))
    subprocess.check_call(cmd)


env.AddCustomTarget(
    name="mergedbin",
    dependencies=None,
    actions=[_merge_bins],
    title="Build merged firmware+SPIFFS",
    description="Create one .bin for ESP Web Tools (bootloader+partitions+app+spiffs).",
)

env.AddCustomTarget(
    name="upload_merged",
    dependencies=None,
    actions=[_merge_bins, _upload_merged],
    title="Flash merged firmware+SPIFFS",
    description="Merge and write at 0x0.",
)