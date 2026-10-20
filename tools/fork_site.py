#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The fork's GitHub Pages site (SeattleUser0/skiff), made by .github/workflows/release.yml after each build:

  index.html                  a page linking the two below
  webapp/installer/index.html web/index_pkg.html with this build's package (as web/make_site.py makes it)
  webapp/editor/index.html    web/editor.html (+ its fonts, fukiai.ttf, fm1backup.js): the per-note editor
  firmware/skiff-VER.fwsc     the package, with LICENSE, LICENSING.md, LICENSES/

web/make_site.py also needs the browser emulator (Emscripten), which the fork's build does not make; this keeps only
the installer and the editor. It calls make_site.py's own pieces, so both stay the same where they overlap.

  tools/fork_site.py build/skiff-1.3.3-notes.fwsc 1.3.3-notes OUT_DIR

Rev 1 (fork 1.3.3-notes): first version.
Rev 2: the index page says what the fork is and how it differs from Skiff 1.3.3.
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
sys.path.insert(0, str(WEB))
import make_site as ms  # noqa: E402  (product_of, strip_module)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = ms.product_of(raw)
    if not re.fullmatch(r"FM-1_7\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Skiff package (FM-1_7xx)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no Felucca loader in it (a package patched from an official one carries vendor files)")
    html = (WEB / "index_pkg.html").read_text(encoding="utf-8")
    lib = "\n".join(ms.strip_module((WEB / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js", "fm1backup.js"))
    name = f"skiff-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    (fw / "LICENSES").mkdir(exist_ok=True)
    for f in (ROOT / "LICENSES").glob("*.txt"):
        shutil.copy(f, fw / "LICENSES" / f.name)
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    shutil.copy(WEB / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (WEB / f).exists():
            shutil.copy(WEB / f, ed / f)
    if (WEB / "fonts").is_dir():
        shutil.copytree(WEB / "fonts", ed / "fonts", dirs_exist_ok=True, ignore=shutil.ignore_patterns("InterTight*"))
    (out / "index.html").write_text(               # ((rev 2) what the fork is and how it differs, as README.md)
        '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">'
        f'<title>Skiff {version} fork</title>'
        '<style>body{font:16px/1.45 system-ui,sans-serif;max-width:46em;margin:2em auto;padding:0 16px;'
        'background:#fff;color:#111}@media(prefers-color-scheme:dark){body{background:#111;color:#eee}a{color:#8bf}}'
        'li{margin:.3em 0}</style>'
        f'<h1>Skiff {version}</h1>'
        '<p><b>This is a fork, not the official Skiff.</b> It is <a href="https://github.com/illuminus5/skiff">Skiff</a> '
        '1.3.3 by illuminus5 with per-note step modes added. The official Skiff is at '
        '<a href="https://illuminus5.github.io/skiff/">illuminus5.github.io/skiff</a>.</p>'
        '<ul><li><a href="webapp/installer/">Installer</a> (Chrome or Edge on a computer)</li>'
        '<li><a href="webapp/editor/">Editor</a> (Chrome, Edge, or MIDIWeb on the iPad)</li>'
        f'<li><a href="firmware/{name}">{name}</a></li></ul>'
        '<h2>How it differs from Skiff 1.3.3</h2><ul>'
        '<li>Each note in a step has its own mode: - ACC SLD A+S TIE. TIE keeps the note sounding through the next '
        'step with no new attack; SLD glides its voice to the closest note of the next step.</li>'
        '<li>A step SLIDE on a POLY track holds the notes the next step repeats with no new attack.</li>'
        '<li>New SEQ &gt; NOTES page after ROLL: KNOB 1 step, KNOB 2 note, KNOB 3 pitch, KNOB 4 mode.</li>'
        '<li>The editor shows a cell for each note, with its pitch and its mode.</li>'
        '<li>A block of 4 patterns holding a note mode is saved in a bigger format the official Skiff cannot read. '
        'Other patterns are saved as before.</li>'
        '<li>No USB serial console (a developer tool), and 7 undo levels instead of 8, to fit the FM-1.</li></ul>'
        '<p>Source and details: <a href="https://github.com/SeattleUser0/skiff">github.com/SeattleUser0/skiff</a> '
        '(README, FORK.md, SPEC-PERNOTE.md).</p>\n', encoding="utf-8")
    print(f"site: {out}: webapp/installer, webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
