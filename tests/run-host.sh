#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
CC_BIN="${CC:-cc}"
COMMON=(-std=gnu11 -Wall -Wextra -Werror -Isrc)
ENGINE=(src/l10n.c src/config.c src/calc.c src/tools.c src/aero.c src/symbolic.c src/dimensions.c src/aero_state.c src/analyze.c src/store.c)
SAN=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
"$CC_BIN" "${COMMON[@]}" -O2 -fPIC -shared "${ENGINE[@]}" -lm -o build/libaero.so
cp build/libaero.so build/libcalc.so
python3 tests/verify_numeric.py
python3 tests/verify_extensions.py
for test in app upgrade extensions config; do
    "$CC_BIN" "${COMMON[@]}" "${SAN[@]}" "tests/test_${test}.c" "${ENGINE[@]}" src/app.c src/aero_export.c src/selftest.c -lm -o "build/test-${test}"
    ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "build/test-${test}"
done
python3 tests/verify_exports.py
if command -v pkg-config >/dev/null 2>&1; then
    read -r -a FONT_CFLAGS <<< "$(pkg-config --cflags freetype2 harfbuzz)"
    read -r -a FONT_LIBS <<< "$(pkg-config --libs freetype2 harfbuzz)"
else
    FONT_INCLUDE="${HOST_FONT_INCLUDE:-/usr/include}"
    FONT_CFLAGS=(-I"$FONT_INCLUDE/freetype2" -I"$FONT_INCLUDE/harfbuzz")
    read -r -a FONT_LIBS <<< "${HOST_FONT_LIBS:--lfreetype -lharfbuzz}"
fi
"$CC_BIN" "${COMMON[@]}" "${SAN[@]}" "${FONT_CFLAGS[@]}" tests/preview.c "${ENGINE[@]}" src/app.c src/ui.c src/mathview.c src/gfx.c src/font.c src/unicode_font.c "${FONT_LIBS[@]}" -lm -o build/preview
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build/preview
