#!/usr/bin/env python3
"""Install optional Chinese text/fonts from pinned external sources or a local pack.

No translation, font, cameo or retail game data is bundled with this script.
Supply an explicit cache directory for downloads and a new backup directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import urllib.request

SOURCE = "https://github.com/Translate-with-LOVE/Ra2-zh_hans-main"
REVISION = "f09640b65b8540c2cf3fe779ee2cc0e0a4fc15f2"
INPUTS = {
    "csf/ra2.zh_Hans.json": "38210bf044951bdc10dbcd761706951f53ad5e8430da959023ea2086f26424ab",
    "csf/ra2md.zh_Hans.json": "762a297e5a235140c6fac61337d8b80ff6312865a0ec4463378d8cd5616c15df",
    "fnt/game.fnt": "9488ee6153a0802f387b91a7283fec53514ca910d29994be99ab3197f77df2f4",
}
OUTPUTS = {
    "ra2.csf": "8b7c92d3d513ce3f7f0ee1a0b8271dc75f27a7ca457aa9ce5f9f1dd89cd93e7e",
    "ra2md.csf": "194561721869cfb34fae31ba6da1b7390314993134e5ee22ada011538569279d",
    "game.fnt": INPUTS["fnt/game.fnt"],
    "ecache99.mix": "e1f8812e498e82b89f3bb0182e1790e0578f88839416a0cc4dc2945434d93aa5",
    "ecachemd99.mix": "bc00a6d06c2cdaa44a25eb630410e4676500dad02543d4f851cd7e48dacb0dcf",
}


def checked(data, expected, name):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("SHA-256 mismatch: " + name)
    return data


def build_csf(labels):
    """CSF v3: ASCII labels and complemented UTF-16LE string code units."""
    parts = [struct.pack("<4s5I", b" FSC", 3, len(labels), len(labels), 0, 9)]
    for label, entry in labels.items():
        name = label.encode("ascii")
        value = entry["Value"].encode("utf-16le")
        extra = entry.get("Extra")
        parts += [struct.pack("<4sII", b" LBL", 1, len(name)), name,
                  struct.pack("<4sI", b" RTS" if extra is None else b"WRTS", len(value) // 2),
                  bytes(b ^ 0xFF for b in value)]
        if extra is not None:
            extra = extra.encode("ascii")
            parts += [struct.pack("<I", len(extra)), extra]
    return b"".join(parts)


def fetch(path, cache):
    target = cache / path
    if target.exists():
        return checked(target.read_bytes(), INPUTS[path], path)
    url = SOURCE.replace("github.com", "raw.githubusercontent.com") + "/" + REVISION + "/" + path
    with urllib.request.urlopen(url, timeout=60) as response:
        data = checked(response.read(), INPUTS[path], path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    return data


def payloads(cache=None, pack=None):
    if pack is not None:
        # The complete locally built pack adds translated cameo artwork.
        return {name: checked((pack / name).read_bytes(), digest, name)
                for name, digest in OUTPUTS.items()}
    data = {"game.fnt": fetch("fnt/game.fnt", cache)}
    for name in ("ra2", "ra2md"):
        raw = fetch("csf/" + name + ".zh_Hans.json", cache)
        compiled = build_csf(json.loads(raw))
        data[name + ".csf"] = checked(compiled, OUTPUTS[name + ".csf"], name + ".csf")
    return data


def install(game, backup, data):
    if not game.is_dir() or not any(p.name.lower() in ("game.exe", "gamemd.exe") for p in game.iterdir()):
        raise ValueError("point --game at your installed game folder")
    marker = "Localization/zh-Hans-source.json"
    metadata = {"source": SOURCE, "revision": REVISION, "language": "zh-Hans", "audio": "original",
                "files": {name: hashlib.sha256(value).hexdigest() for name, value in data.items()}}
    data = {**data, marker: (json.dumps(metadata, indent=2) + "\n").encode()}
    # Refuse ambiguous case variants: the native host uses case-insensitive lookup.
    targets = {}
    for name in data:
        destination = game / name
        matches = [p for p in destination.parent.iterdir() if p.name.lower() == destination.name.lower()] if destination.parent.exists() else []
        if len(matches) > 1:
            raise ValueError("multiple case variants of " + name)
        targets[name] = matches[0] if matches else destination
    backup.mkdir(parents=True, exist_ok=False)
    manifest = {"game": str(game), "files": {}}
    for name, destination in targets.items():
        manifest["files"][str(destination.relative_to(game))] = destination.exists()
        if destination.exists():
            saved = backup / destination.relative_to(game)
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(destination, saved)
    (backup / "restore.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for name, value in data.items():
        destination = targets[name]
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(value)
    print("Installed", len(data) - 1, "localization files in", game)
    print("Previous files and restore manifest:", backup)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--backup-dir", type=Path, required=True, help="a new directory for original files and restore.json")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--cache-dir", type=Path, help="download pinned text and font inputs here")
    source.add_argument("--pack-dir", type=Path, help="a complete verified five-file local overlay pack")
    args = parser.parse_args()
    try:
        install(args.game.resolve(), args.backup_dir.resolve(), payloads(args.cache_dir, args.pack_dir))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
