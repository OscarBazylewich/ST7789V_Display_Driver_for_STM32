# This script does the tests of the ST7789 HAL library without hardware.
# It compiles the tests with a GCC for the CPU of the PC, and operates them.
# It compiles the tests one time for each configuration in VARIANTS, for
# example with different values of ST7789HAL_BUFFER_PIXELS.
#
# To use the script, give this command:
#   `bash Lib/st7789_hal/test/run_tests.sh [compiler]`
#
# The default compiler is `gcc`. The executables and the results go into
# the folder `Lib/st7789_hal/test/build`. The script returns 0 if all the
# tests are correct. If not, it returns 1.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$(cd "$HERE/.." && pwd)"
OLD="$(cd "$HERE/../../st7789" && pwd)"
OUT="$HERE/build"
CC="${1:-gcc}"

mkdir -p "$OUT"

CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g)

# `-fsanitize=undefined` finds incorrect C operations, for example an
# integer result that is too large for its type. MinGW does not have the library for this
# function. Thus, if necessary, the script uses
# `-fsanitize-undefined-trap-on-error`. Then the executable stops at an
# error and does not show a message.
probe="$OUT/ubsan_probe.c"
echo 'int main(void) { return 0; }' > "$probe"
if "$CC" -fsanitize=undefined "$probe" -o "$OUT/ubsan_probe.exe" >/dev/null 2>&1; then
  SAN=(-fsanitize=undefined -fno-sanitize-recover=all)
  echo "UBSan: on (with its run-time library)"
elif "$CC" -fsanitize=undefined -fsanitize-undefined-trap-on-error "$probe" \
     -o "$OUT/ubsan_probe.exe" >/dev/null 2>&1; then
  SAN=(-fsanitize=undefined -fsanitize-undefined-trap-on-error)
  echo "UBSan: on (trap mode: an error stops the program without a message)"
else
  SAN=()
  echo "UBSan: off (the compiler does not support it)"
fi
rm -f "$probe" "$OUT/ubsan_probe.exe"

SOURCES=(
  "$HERE/test_main.c"
  "$HERE/st7789_model.c"
  "$HERE/reference.c"
  "$HERE/fake_hal/fake_hal.c"
  "$LIB/st7789_hal.c"
  "$LIB/st7789_hal_gfx.c"
  "$LIB/st7789_hal_font5x7.c"
  "$OLD/st7789_font5x7.c"
)
INCLUDES=(-I"$HERE/fake_hal" -I"$HERE" -I"$LIB" -I"$OLD")

# The name and the compiler flags of each configuration
VARIANTS=(
  "buf16|-DST7789HAL_BUFFER_PIXELS=16"
  "buf512|"
  "buf4096|-DST7789HAL_BUFFER_PIXELS=4096"
  "buf32752|-DST7789HAL_BUFFER_PIXELS=32752"
  "buf512_unsigned_char_const_hal|-funsigned-char -DFAKE_HAL_CONST_PDATA"
  "buf64_dma32|-DST7789HAL_BUFFER_PIXELS=64 -DST7789HAL_DMA_MIN_BYTES=32"
)

failed=0
summary=""
for v in "${VARIANTS[@]}"; do
  name="${v%%|*}"
  defs="${v#*|}"
  exe="$OUT/st7789_hal_test_$name.exe"
  echo
  echo "=== $name: compile ($defs)"
  if ! "$CC" "${CFLAGS[@]}" ${SAN[@]+"${SAN[@]}"} $defs "${INCLUDES[@]}" "${SOURCES[@]}" -o "$exe" \
       2> "$OUT/$name.compile.log"; then
    cat "$OUT/$name.compile.log"
    echo "=== $name: COMPILE ERROR"
    summary="$summary\n  $name: COMPILE ERROR"
    failed=1
    continue
  fi
  if [ -s "$OUT/$name.compile.log" ]; then
    cat "$OUT/$name.compile.log"
  fi
  echo "=== $name: run"
  "$exe" > "$OUT/$name.log" 2>&1
  rc=$?
  grep -v '^\[ RUN  \]' "$OUT/$name.log"
  if [ $rc -ne 0 ]; then
    echo "=== $name: FAIL (exit code $rc, the full output is in $OUT/$name.log)"
    summary="$summary\n  $name: FAIL (exit code $rc)"
    failed=1
  else
    summary="$summary\n  $name: $(tail -n 2 "$OUT/$name.log" | head -n 1)"
  fi
done

echo
echo "=== Summary"
printf "%b\n" "$summary"
if [ $failed -ne 0 ]; then
  echo "RESULT: FAIL"
  exit 1
fi
echo "RESULT: PASS"
exit 0
