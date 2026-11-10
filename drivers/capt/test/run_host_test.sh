#!/bin/bash
# Builds captjob for the host and runs it against fake_printer.py.
set -e
cd "$(dirname "$0")"
D=..; OUT=$(mktemp -d)
clang -std=gnu99 -O1 -w -D_DARWIN_C_SOURCE -I. -I$D -I$D/captdriver \
  $D/capt_main.c $D/captdriver/{capt-command,capt-status,generic-ops,hiscoa-common,hiscoa-compress,paper,printer,prn_lbp2900}.c -o "$OUT/captjob"
"$OUT/captjob" probe "MFG:Canon;CMD:CAPT;MDL:LBP6000/LBP6018;" 2>/dev/null && echo "probe LBP6000: ok"
"$OUT/captjob" probe "MFG:Canon;CMD:CAPT;MDL:LBP6020;" 2>/dev/null && { echo "probe LBP6020 should fail"; exit 1; } || echo "probe LBP6020: unknown, as expected"
python3 fake_printer.py "$OUT/captjob" "MFG:Canon;CMD:CAPT;MDL:LBP6000/LBP6018;"
