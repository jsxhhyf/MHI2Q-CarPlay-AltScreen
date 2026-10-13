#!/bin/sh
# Host test for Toolbox/scripts/altscreen_v33_rgi_config.sh on synthetic
# dio_manager.json shapes.  Runs the helper under every POSIX shell given in
# SHELLS (default: /bin/sh and /bin/dash when present).
#   sh Tools/test_rgi_config.sh [extra_real_dio_manager.json ...]
# Extra files are only classified (apply on a copy, verify), never modified.
set -u
PROJECT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CFG="$PROJECT/Toolbox/scripts/altscreen_v33_rgi_config.sh"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fails=0

mk() {  # mk NAME SENT_ARRAY_OR_- RECEIVED_ARRAY_OR_-
  {
    echo '{'
    echo '    "iap2": {'
    echo '        "Name": "MMI",'
    [ "$2" = - ] || printf '        "MessagesSentByAccessory": [%s],\n' "$2"
    [ "$3" = - ] || printf '        "MessagesReceivedFromDevice": [%s],\n' "$3"
    echo '        "PowerProvidingCapability": 2'
    echo '    }'
    echo '}'
  } > "$T/$1.json"
}

expect() {  # expect SHELL ACTION FILE RC PATTERN [unchanged]
  sh_=$1 act=$2 f=$3 rc_want=$4 pat=$5
  cp "$f" "$T/before"
  out=$("$sh_" "$CFG" "$act" "$f" 2>&1); rc=$?
  ok=1
  [ "$rc" = "$rc_want" ] || ok=0
  printf '%s\n' "$out" | grep -q -- "$pat" || ok=0
  if [ "${6:-}" = unchanged ] && ! cmp -s "$T/before" "$f"; then ok=0; fi
  if [ $ok = 1 ]; then echo "ok   [$sh_] $act $(basename "$f") -> $out"
  else echo "FAIL [$sh_] $act $(basename "$f") rc=$rc (want $rc_want, /$pat/) -> $out"; fails=$((fails+1)); fi
}

SHELLS=${SHELLS:-"/bin/sh $( [ -x /bin/dash ] && echo /bin/dash )"}
BASE_S='"0x5000", "0xAE00"'
BASE_R='"0x5001", "0xAE01"'
for sh_ in $SHELLS; do
  mk no_arrays - -
  expect "$sh_" apply  "$T/no_arrays.json" 0 'UNCHANGED state=NO_ARRAYS' unchanged
  expect "$sh_" verify "$T/no_arrays.json" 0 'PASS state=NO_ARRAYS' unchanged

  mk absent "$BASE_S" "$BASE_R"
  expect "$sh_" verify "$T/absent.json" 1 'FAIL state=ABSENT' unchanged
  expect "$sh_" apply  "$T/absent.json" 0 'PASS state=PATCHED'
  expect "$sh_" verify "$T/absent.json" 0 'PASS state=COMPLETE' unchanged
  expect "$sh_" apply  "$T/absent.json" 0 'UNCHANGED state=COMPLETE' unchanged
  grep -q '"0x5200", "0x5203"' "$T/absent.json" && grep -q '"0x5201", "0x5202", "0x5204"' "$T/absent.json" ||
    { echo "FAIL [$sh_] patched ids not found"; fails=$((fails+1)); }

  mk only_sent "$BASE_S" -
  expect "$sh_" apply  "$T/only_sent.json" 1 'REFUSED state=INVALID' unchanged
  expect "$sh_" verify "$T/only_sent.json" 1 'FAIL state=INVALID' unchanged

  mk only_recv - "$BASE_R"
  expect "$sh_" apply  "$T/only_recv.json" 1 'REFUSED state=INVALID' unchanged

  mk mixed "$BASE_S, \"0x5200\"" "$BASE_R"
  expect "$sh_" apply  "$T/mixed.json" 1 'REFUSED state=MIXED' unchanged

  mk dup "\"0x5200\", \"0x5200\", \"0x5203\"" '"0x5201", "0x5202", "0x5204"'
  expect "$sh_" apply  "$T/dup.json" 1 'REFUSED state=INVALID' unchanged
done

for real in "$@"; do
  cp "$real" "$T/real.json"
  for sh_ in $SHELLS; do
    out=$("$sh_" "$CFG" apply "$T/real.json" 2>&1); rc=$?
    cmp -s "$real" "$T/real.json" && same=unchanged || same=modified
    vout=$("$sh_" "$CFG" verify "$T/real.json" 2>&1); vrc=$?
    echo "real [$sh_] $(basename "$real"): apply rc=$rc $out ($same); verify rc=$vrc $vout"
    cp "$real" "$T/real.json"
  done
done

[ "$fails" = 0 ] && echo "RGI_CONFIG_TEST=PASS" || echo "RGI_CONFIG_TEST=FAIL failures=$fails"
[ "$fails" = 0 ]
