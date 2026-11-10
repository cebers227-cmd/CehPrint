#!/usr/bin/env python3
"""Drives captjob (host build) against a scripted CAPT printer.

usage: fake_printer.py <captjob-binary> <ieee-id>
Builds a 2-page 1-bit test job, answers every command like a ready printer and
checks that the job finishes, pages are counted and raster data was sent.
"""
import os, struct, subprocess, sys, tempfile

binary, ieee = sys.argv[1], sys.argv[2]
W, H, BPL, PAGES = 4960, 7016, 620, 2
with tempfile.NamedTemporaryFile(delete=False) as f:
    for p in range(PAGES):
        rows = bytearray()
        for y in range(H):
            rows += (b"\xff" * BPL) if (y // 200) % 2 == 0 else (b"\x00" * BPL)
        f.write(rows)
    pages = f.name

proc = subprocess.Popen(
    [binary, "job", ieee, pages, str(PAGES), str(W), str(H), str(BPL), "600", "595", "842"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

NOREPLY = {0xC0A0, 0xC0A4, 0xD0A9}
STATUS = {0xE0A0, 0xA0A8}
inbuf, outq = bytearray(), bytearray()
pages_done = 0
data_bytes = 0
cmds = []

def status_payload():
    s = bytearray(44)
    for off in (14, 16, 18, 20, 34):
        s[off:off + 2] = struct.pack("<H", pages_done)
    return bytes(s)

def reply(cmd, payload):
    outq.extend(struct.pack("<HH", cmd, 4 + len(payload)) + payload)

def process():
    global pages_done, data_bytes
    while len(inbuf) >= 4:
        cmd, total = struct.unpack_from("<HH", inbuf)
        if len(inbuf) < total:
            return
        body = bytes(inbuf[4:total]); del inbuf[:total]
        cmds.append(cmd)
        if cmd == 0xC0A0: data_bytes += len(body)
        if cmd == 0xC0A4: pages_done += 1
        if cmd in NOREPLY: continue
        if cmd in STATUS: reply(cmd, status_payload())
        elif cmd == 0xA2A0: reply(cmd, b"\x00\x00\x01\x00")
        else: reply(cmd, b"\x00\x00")

def rd(n):
    b = proc.stdout.read(n)
    if len(b) != n: raise EOFError
    return b

try:
    while True:
        tag = proc.stdout.read(1)
        if not tag: break
        (ln,) = struct.unpack("<I", rd(4))
        if tag == b"W":
            inbuf.extend(rd(ln)); process()
            proc.stdin.write(struct.pack("<i", ln)); proc.stdin.flush()
        elif tag == b"R":
            rd(4)
            chunk = bytes(outq[:ln]); del outq[:len(chunk)]
            proc.stdin.write(struct.pack("<i", len(chunk) if chunk else -1) + chunk); proc.stdin.flush()
        else:
            print("bad tag", tag); break
except EOFError:
    pass
code = proc.wait()
os.unlink(pages)
print(f"exit={code} pages_acked={pages_done} data_bytes={data_bytes} commands={len(cmds)}")
ok = code == 0 and pages_done == PAGES and data_bytes > 0
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
