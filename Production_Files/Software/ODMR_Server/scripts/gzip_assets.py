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
COMPRESS_EXT = (".html", ".css", ".js", ".json", ".svg", ".ico", ".txt", ".xml", ".map")


def _gzip_one(src, dst):
    with open(src, "rb") as f_in, gzip.open(dst, "wb", compresslevel=9) as f_out:
        shutil.copyfileobj(f_in, f_out)


def gzip_assets(*_args, **_kwargs):
    if not os.path.isdir(DATA_DIR):
        print("gzip_assets: no data/ directory, skipping")
        return

    total_in = total_out = 0
    for name in sorted(os.listdir(DATA_DIR)):
        src = os.path.join(DATA_DIR, name)
        if not os.path.isfile(src) or name.endswith(".gz"):
            continue
        if os.path.splitext(name)[1].lower() not in COMPRESS_EXT:
            continue

        dst = src + ".gz"
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
