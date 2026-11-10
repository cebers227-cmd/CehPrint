# Third-party licenses

CehPrint embeds and ports code from several open-source projects.
This file lists each upstream and the license under which we use it.

## ZXing — Apache License 2.0

`com.google.zxing:core:3.5.3` — used for QR code generation and live
camera decoding.  See https://github.com/zxing/zxing.

## AndroidX (CameraX, Compose, DataStore, Room, etc.) — Apache License 2.0

The complete list is mirrored from `gradle/libs.versions.toml`.  All
Jetpack libraries ship under Apache-2.0.  See https://source.android.com/license.

## Kotlin / Coroutines — Apache License 2.0

JetBrains Kotlin standard library and `kotlinx-coroutines`.

## CUPS (libcups, client part) — Apache License 2.0

Statically linked into the bundled print-driver executables (`libcehraster.so`, `librastertobrlaser.so`).
Source: https://github.com/OpenPrinting/cups, see `drivers/SOURCES.md` for the exact revision.

## brlaser — GPL-2.0-or-later

Bundled as `librastertobrlaser.so` (Brother laser printers). Distributed under GPL-3.0-or-later as part of CehPrint.
Source: https://github.com/pdewacht/brlaser, see `drivers/SOURCES.md` for the exact revision.

## SpliX and jbigkit — GPL-2.0-only / GPL-2.0-or-later

Bundled as `librastertoqpdl.so` (Samsung, Xerox, Dell, HP, Lexmark, Toshiba SPL printers). jbigkit is linked into it.
SpliX runs as a separate process started by the app and talks to it through pipes only.
The patch `drivers/patches/splix-0001-deterministic.patch` is part of the source. See `drivers/SOURCES.md` for revisions.

## Gutenprint — GPL-2.0-or-later

Bundled as `librastertogutenprint.so` and `libgutenprintppd.so` (Canon, Epson and other inkjet/laser/dye-sub printers), with its XML data in `assets/drivers/gutenprint-xml.zip`.
Runs as separate processes started by the app. Source: see `drivers/SOURCES.md`.

## HPLIP (hpcups) and libjpeg-turbo — GPL-2.0-or-later, MIT/BSD, BSD-3-Clause/IJG/zlib

Bundled as `libhpcups.so` (HP inkjet and laser printers), runs as a separate process. The closed-source HP plug-in and `libImageProcessor` are not used and not shipped.
libjpeg-turbo is linked into it. See `drivers/SOURCES.md` for revisions and the patch.

## epson-inkjet-printer-escpr — GPL-2.0-or-later

Bundled as `libepson-escpr-wrapper.so` and `libescprcore.so` (Epson inkjet printers), separate processes. See `drivers/SOURCES.md`.

## captdriver — GPL-3.0-or-later (separate program, since v0.26.0)

`drivers/capt/` — Canon CAPT driver (LBP2900/3000/3010/3018/3050/3100/3108/3150/6000/6018),
a port of https://github.com/mounaiban/captdriver (`62719249ac34633338be54bc74beddd0e7003d38`,
itself a fork of https://github.com/agalakhov/captdriver) with CUPS I/O replaced by a pipe
protocol. It is built by `drivers/build.sh` as the executable `libcaptjob.so` and runs as a
separate process; the app talks to it only through stdin/stdout, so it is an aggregate, not
a combined work. Source: this directory in the public repository, licence text in
`drivers/capt/captdriver/LICENSE.GPL3`.

