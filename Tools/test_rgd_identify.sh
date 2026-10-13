#!/bin/sh
# Host tests for rgi_native Identify patching: rgd_identify.c on its own, then
# the whole rgd patcher on the real framework sources.  Needs a cc with
# AddressSanitizer + UBSan (macOS clang works):  sh Tools/test_rgd_identify.sh
set -eu
PROJECT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
R="$PROJECT/Toolbox/carplay_alt_screen/rgi_native"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
${CC:-cc} -std=gnu99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-sanitize-recover=all -o "$OUT/test_rgd_identify" \
    "$R/tests/test_rgd_identify.c" "$R/src/routeguidance/rgd_identify.c"
"$OUT/test_rgd_identify"

# Whole patcher on the real framework + routeguidance sources (host build).
SRC="$R/src"
${CC:-cc} -std=gnu99 -O1 -g -Wall -Wno-unused-function -fsanitize=address,undefined \
    -fno-sanitize-recover=all -I"$SRC" -o "$OUT/test_rgd_patcher" "$R/tests/test_rgd_patcher.c" \
    "$SRC/framework/logging.c" "$SRC/framework/state_trace.c" "$SRC/framework/signal_guard.c" \
    "$SRC/framework/bus.c" "$SRC/framework/iap2_protocol.c" "$SRC/framework/hook_framework.c" \
    "$SRC/routeguidance/rgd_tlv.c" "$SRC/routeguidance/rgd_identify.c" "$SRC/routeguidance/rgd_hook.c" \
    -lpthread
"$OUT/test_rgd_patcher" "$OUT/rgi.log"
# The diagnostics the first car test depends on must actually be written.
for line in 'Identify raw @0/' 'Identify as built: len=' 'Identify RGI message ids added: sent +2 received +3' \
            'Identify as sent: len=' 'Identify message lists unusable (sent=-1 received=3)'; do
  grep -qF "$line" "$OUT/rgi.log" || { echo "RGD_PATCHER_LOG=FAIL missing: $line"; exit 1; }
done
echo "RGD_PATCHER_LOG=PASS"
