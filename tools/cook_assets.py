#!/usr/bin/env python3
"""Minimal asset cook step: copy raw assets and emit a manifest."""

from __future__ import annotations

import argparse
import json
import shutil
import uuid
from pathlib import Path


def load_metadata(meta_path: Path) -> dict:
    if not meta_path.exists():
        return {}
    with meta_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_metadata(meta_path: Path, asset_id: str, asset_type: str, source: str) -> None:
    meta = {
        "version": 1,
        "id": asset_id,
        "type": asset_type,
        "source": source,
    }
    with meta_path.open("w", encoding="utf-8") as handle:
        json.dump(meta, handle, indent=2)
        handle.write("\n")


def classify_asset_type(path: Path) -> str:
    ext = path.suffix.lower()
    if ext in {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif", ".dds", ".ktx", ".ktx2"}:
        return "Texture"
    if ext in {".gltf", ".glb", ".obj", ".fbx", ".dae"}:
        return "Mesh"
    if ext in {".wav", ".mp3", ".ogg", ".flac", ".aiff"}:
        return "Audio"
    if ext in {".lua", ".py", ".js", ".cs"}:
        return "Script"
    if ext in {".vert", ".frag", ".glsl", ".spv"}:
        return "Shader"
    if ext == ".json":
        return "Scene" if "scenes" in path.parts else "Other"
    return "Other"


def main() -> int:
    parser = argparse.ArgumentParser(description="Cook raw assets into a runtime-ready folder.")
    parser.add_argument("--assets", default="assets", help="Assets directory (repo-relative)")
    parser.add_argument("--out", default="build/cooked", help="Output directory")
    parser.add_argument("--manifest", default="asset_index.json", help="Manifest filename")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    assets_dir = (repo_root / args.assets).resolve()
    out_dir = (repo_root / args.out).resolve()

    if not assets_dir.exists():
        raise SystemExit(f"Assets directory not found: {assets_dir}")

    out_dir.mkdir(parents=True, exist_ok=True)
    manifest_entries = []

    for asset_path in assets_dir.rglob("*"):
        if not asset_path.is_file():
            continue
        if asset_path.name.startswith("."):
            continue
        if asset_path.name.endswith(".asset.json"):
            continue

        relative = asset_path.relative_to(assets_dir)
        meta_path = asset_path.with_suffix(asset_path.suffix + ".asset.json")
        meta = load_metadata(meta_path)

        asset_id = meta.get("id") or str(uuid.uuid4())
        asset_type = meta.get("type") or classify_asset_type(asset_path)
        source = meta.get("source") or relative.as_posix()

        if not meta:
            write_metadata(meta_path, asset_id, asset_type, source)

        manifest_entries.append({
            "id": asset_id,
            "path": relative.as_posix(),
            "type": asset_type,
        })

        target_path = out_dir / relative
        target_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(asset_path, target_path)

    manifest_path = out_dir / args.manifest
    with manifest_path.open("w", encoding="utf-8") as handle:
        json.dump({"assets": manifest_entries}, handle, indent=2)
        handle.write("\n")

    print(f"Cooked {len(manifest_entries)} assets to {out_dir}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
