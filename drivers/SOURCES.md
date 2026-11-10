# Driver sources

Binaries in `app/src/main/jniLibs/*/` are built by `drivers/build.sh` from these upstream sources.
Clone them into `drivers/src/` at the pinned revisions before building.

| Component | Upstream | Revision | License |
|-----------|----------|----------|---------|
| libcups (static, client part only) | https://github.com/OpenPrinting/cups | v2.4.14 (`2190813d4e78690806f4033c1f120f8e01e69e07`) | Apache-2.0 |
| brlaser (`rastertobrlaser`) | https://github.com/pdewacht/brlaser | `2a49e3287c70c254e7e3ac9dabe9d6a07218c3fa` | GPL-2.0-or-later |
| SpliX (`rastertoqpdl`, Samsung/Xerox/Dell/HP/Lexmark/Toshiba SPL) | https://github.com/OpenPrinting/splix | `7a802180c7b50802aaa16bbd2b3e326237286730` + `drivers/patches/splix-0001-deterministic.patch` | GPL-2.0-only |
| jbigkit (`libjbig85`, linked into SpliX) | https://www.cl.cam.ac.uk/~mgk25/jbigkit/ | 2.1 | GPL-2.0-or-later |
| Gutenprint (`rastertogutenprint`, `libgutenprintppd.so`; Canon, Epson and others) | https://sourceforge.net/projects/gimp-print/files/gutenprint-5.3/5.3.4/ | 5.3.4 (`gutenprint-5.3.4.tar.xz`, sha256 `db44a701d2b8e6a8931c83cec06c91226be266d23e5c189d20a39dd175f2023b`) | GPL-2.0-or-later |
| HPLIP (`hpcups`, HP inkjet and laser) | https://sourceforge.net/projects/hplip/files/hplip/3.22.10/ | 3.22.10 (`hplip-3.22.10.tar.gz`, sha256 `533c3f2f6b53e4163ded4fd81d1f11ae6162a0f6451bd5e62a8382d0c1366624`) + `drivers/patches/hplip-0001-case-sensitive-include.patch` | GPL-2.0-or-later (hpcups), MIT/BSD for parts |
| libjpeg-turbo (linked into hpcups) | https://github.com/libjpeg-turbo/libjpeg-turbo | 2.1.5.1 (sha256 `61846251941e5791005fb7face196eec24541fce04f12570c308557529e92c75`) | BSD-3-Clause / IJG / zlib |
| epson-inkjet-printer-escpr (`epson-escpr-wrapper` + `libescprcore.so`; Epson ESC/P-R) | http://deb.debian.org/debian/pool/main/e/epson-inkjet-printer-escpr/ (orig tarball) `epson-inkjet-printer-escpr_1.7.17.orig.tar.gz` | 1.7.17 (sha256 `8f13f5344f50399bd49e8871b84bd379e9f2957d97f47a044449f959689eecb4`) + `drivers/patches/escpr-0001-filter-dir.patch` | GPL-2.0-or-later |
| cehraster (`libcehraster.so`) | this repository, `drivers/cehraster/` | — | GPL-3.0-or-later |

PPD files in `drivers/ppd/brlaser/` are generated with `ppdc` from `brlaser.drv` (Debian `printer-driver-brlaser` 6-3).
`drivers/make_index.py drivers/ppd` copies them into `app/src/main/assets/drivers/` and writes `index.tsv`.

`drivers/ppd/splix/` holds the 92 raster-input PPDs of SpliX (the 12 PostScript-input PPDs are skipped). Colour profile (CMS) files are not bundled; colour models print without them.
The SpliX patch zero-fills buffers and makes one sort comparator deterministic, so output is identical on every C library.

Gutenprint: PPDs are generated on the phone by `libgutenprintppd.so cat` from `drivers/gutenprint-list.txt` (3578 models); the XML data is shipped as `assets/drivers/gutenprint-xml.zip` and unpacked on first use. `compat/gp_i18n.c` replaces gettext/iconv code (iconv is missing on API 23).

HPLIP: built without the closed-source `libImageProcessor` (`-DDISABLE_IMAGEPROCESSOR`) and without D-Bus. `drivers/ppd/hplip/` holds the 768 PPDs from `hpcups.drv` (`ppdc`); the 78 models that need the closed HP plug-in are left out.

escpr: 586 PPDs from the upstream package in `drivers/ppd/escpr/`. The patch makes the wrapper read the PPD from `$PPD` and the core filter from `$CEH_FILTER_DIR` (it asked CUPS for the PPD before).

Canon CAPT (LBP6000/LBP6018, LBP2900/3000/3010 and others): `drivers/capt/captdriver/` is a copy of https://github.com/mounaiban/captdriver (`62719249ac34633338be54bc74beddd0e7003d38`, GPL-3.0-or-later) with the CUPS I/O replaced by a pipe protocol (`capt-command.c`) and `exit()` replaced by `longjmp` (`printer.c`). `drivers/capt/capt_main.c` is the executable `libcaptjob.so`, built by `drivers/build.sh`. The app picks the printer by the IEEE 1284 `MDL` string, not by USB product id. `drivers/capt/test/run_host_test.sh` runs it against a scripted fake printer.
