#
# PlatformIO pre-build hook: gzip the static web assets in data/ so the
# firmware can serve them with Content-Encoding: gzip via serveStatic().
#
# For every compressible file in data/ (html/css/js/json/svg/ico/txt/xml/map)
# a sibling <name>.gz is (re)generated whenever the source is newer. Both the
# original and the .gz are uploaded to SPIFFS; ESPAsyncWebServer's serveStatic
# automatically serves the .gz to browsers that accept gzip (all of them) and
# falls back to the plain file otherwise.
#
# Binary assets that are already compressed (png/jpg) are left untouched.
#
# Wired up in platformio.ini:  extra_scripts = pre:scripts/gzip_assets.py
#
import os
import gzip
import shutil

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)

DATA_DIR = os.path.join(env.subst("$PROJECT_DIR"), "data")  # noqa: F821
SRC_DIR = os.path.join(env.subst("$PROJECT_DIR"), "src/website_html")  # noqa: F821
# The WebSerial app is served from the board too (/app/, talking HTTP instead of
# Web Serial): its files go flat into data/a/ — SPIFFS names max. 31 characters.
APP_DIR = os.path.join(env.subst("$PROJECT_DIR"), "webserial-app")  # noqa: F821
APP_DATA_DIR = os.path.join(DATA_DIR, "a")

COMPRESS_EXT = (".html", ".css", ".js", ".json", ".svg", ".ico", ".txt", ".xml", ".map")


def _gzip_one(src, dst):
    with open(src, "rb") as f_in, gzip.open(dst, "wb", compresslevel=9) as f_out:
        shutil.copyfileobj(f_in, f_out)


def gzip_assets(*_args, **_kwargs):
    if not os.path.isdir(DATA_DIR):
        print("gzip_assets: no data/ directory, skipping")
        return

    os.makedirs(APP_DATA_DIR, exist_ok=True)
    sources = [(os.path.join(SRC_DIR, n), DATA_DIR) for n in sorted(os.listdir(SRC_DIR))]
    sources.append((os.path.join(APP_DIR, "index.html"), APP_DATA_DIR))
    app_assets = os.path.join(APP_DIR, "assets")
    sources += [(os.path.join(app_assets, n), APP_DATA_DIR) for n in sorted(os.listdir(app_assets))]

    total_in = total_out = 0
    for src, out_dir in sources:
        name = os.path.basename(src)
        dst = os.path.join(out_dir, name + ".gz")
        if not os.path.isfile(src) or name.endswith(".gz"):
            continue
        if os.path.splitext(name)[1].lower() not in COMPRESS_EXT:
            continue

        # Skip if the .gz is already up to date.
        if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
            continue

        try:
            _gzip_one(src, dst)
        except Exception as exc:  # never break the build over a single asset
            print("gzip_assets: failed on %s: %s" % (name, exc))
            continue

        i, o = os.path.getsize(src), os.path.getsize(dst)
        total_in += i
        total_out += o
        print("gzip: %-28s %7d -> %7d bytes (%d%%)" % (name, i, o, 100 - int(100 * o / max(i, 1))))

    if total_in:
        print("gzip_assets: compressed %d -> %d bytes total" % (total_in, total_out))


gzip_assets()
