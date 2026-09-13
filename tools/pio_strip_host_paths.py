# Remap __FILE__ / debug prefixes so firmware .bin assets do not embed
# C:/Users/<host>/.platformio/... from the build machine.
#
# `build_type = release` is not enough: Arduino/ESP_LOG still puts __FILE__
# in .rodata. Wire this as `extra_scripts = pre:tools/pio_strip_host_paths.py`
# on every face.

Import("env")  # noqa: F821 — PlatformIO / SCons

import os
from pathlib import Path


def _slash_variants(path):
    if not path:
        return []
    raw = str(path).rstrip("/\\")
    out = []
    for p in (raw, raw.replace("\\", "/"), raw.replace("/", "\\")):
        if p and p not in out:
            out.append(p)
    return out


def _add_prefix_maps(old, new="."):
    for p in _slash_variants(old):
        env.Append(
            CCFLAGS=[
                "-fmacro-prefix-map=%s=%s" % (p, new),
                "-ffile-prefix-map=%s=%s" % (p, new),
            ]
        )


_add_prefix_maps(env.get("PROJECT_DIR"))
_add_prefix_maps(
    os.environ.get("PLATFORMIO_CORE_DIR") or env.get("PROJECT_CORE_DIR")
)
_add_prefix_maps(str(Path.home()))
