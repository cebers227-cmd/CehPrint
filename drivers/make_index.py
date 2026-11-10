#!/usr/bin/env python3
import pathlib
import re
import shutil
import sys
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
ASSETS = HERE.parent / "app/src/main/assets/drivers"


def field(text, name):
    m = re.search(r'^\*' + re.escape(name) + r':\s*"([^"]*)"', text, re.M)
    return m.group(1).strip() if m else ""


GP_SUFFIX = " - CUPS+Gutenprint v5.3.4"


def gutenprint_rows():
    rows = []
    for line in (HERE / "gutenprint-list.txt").read_text().splitlines():
        m = re.match(r'"gutenprint\.5\.3://([^/"]+)/expert" \w+ "([^"]*)" "([^"]*)" "([^"]*)"', line)
        if not m:
            continue
        key, mfg, name, dev = m.groups()
        name = name.removesuffix(GP_SUFFIX)
        mdl = re.search(r"MDL:([^;]*)", dev)
        rows.append("\t".join([
            "gutenprint", "rastertogutenprint", f"gutenprint/{key}.ppd", mfg, name,
            mdl.group(1).strip() if mdl else "", "",
        ]))
    return rows


def build_gutenprint_zip():
    xml = HERE / "src/gutenprint/src/xml"
    with zipfile.ZipFile(ASSETS / "gutenprint-xml.zip", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(xml.rglob("*.xml")):
            z.write(f, f.relative_to(xml).as_posix())


def main(ppd_root):
    rows = []
    ppd_root = pathlib.Path(ppd_root)
    out_ppd = ASSETS / "ppd"
    if out_ppd.exists():
        shutil.rmtree(out_ppd)
    for driver_dir in sorted(p for p in ppd_root.iterdir() if p.is_dir()):
        for ppd in sorted(driver_dir.glob("*.ppd")):
            text = ppd.read_text(errors="replace")
            filt = re.search(r'^\*cupsFilter2?:\s*"[^"]*?\s(\S+)"', text, re.M)
            if "requires proprietary plugin" in text:
                continue
            if not filt or "vnd.cups-raster" not in filt.group(0):
                continue
            dev = field(text, "1284DeviceID")
            mdl = re.search(r"MDL:([^;]*)", dev)
            cmd = re.search(r"CMD:([^;]*)", dev)
            rel = f"{driver_dir.name}/{ppd.name}"
            (out_ppd / driver_dir.name).mkdir(parents=True, exist_ok=True)
            shutil.copy(ppd, out_ppd / rel)
            rows.append("\t".join([
                driver_dir.name,
                filt.group(1),
                rel,
                field(text, "Manufacturer"),
                field(text, "ModelName"),
                mdl.group(1).strip() if mdl else "",
                cmd.group(1).strip() if cmd else "",
            ]))
    rows += gutenprint_rows()
    build_gutenprint_zip()
    (ASSETS / "index.tsv").write_text("\n".join(rows) + "\n")
    print(len(rows), "models")


if __name__ == "__main__":
    main(sys.argv[1])
