#
# PlatformIO pre-build hook: gzip the web app (webserial-app/) into data/a/ for
# the SPIFFS image. The board serves it at / with Content-Encoding: gzip
# (serveStatic picks <name>.gz), the same app as https://youseetoo.github.io/odmr.
#
# The files go flat into data/a/: SPIFFS names are limited to 31 characters.
# data/ is generated (git-ignored) and rebuilt from scratch on every run, so
# deleted app files never linger in the image.
#
# Wired up in platformio.ini:  extra_scripts = pre:scripts/gzip_assets.py
#
import gzip
import os
import shutil

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
APP_DIR = os.path.join(PROJECT_DIR, "webserial-app")
DATA_DIR = os.path.join(PROJECT_DIR, "data")
OUT_DIR = os.path.join(DATA_DIR, "a")

COMPRESS_EXT = (".html", ".css", ".js", ".json", ".svg", ".ico", ".txt", ".xml")


def gzip_assets():
    shutil.rmtree(DATA_DIR, ignore_errors=True)
    os.makedirs(OUT_DIR)
    sources = [os.path.join(APP_DIR, n) for n in sorted(os.listdir(APP_DIR))]
    sources += [os.path.join(APP_DIR, "assets", n) for n in sorted(os.listdir(os.path.join(APP_DIR, "assets")))]

    total_in = total_out = 0
    for src in sources:
        name = os.path.basename(src)
        if not os.path.isfile(src) or os.path.splitext(name)[1].lower() not in COMPRESS_EXT:
            continue
        dst = os.path.join(OUT_DIR, name + ".gz")
        with open(src, "rb") as f_in, gzip.open(dst, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)
        total_in += os.path.getsize(src)
        total_out += os.path.getsize(dst)
    print("gzip_assets: webserial-app %d -> %d bytes in data/a/" % (total_in, total_out))


gzip_assets()
