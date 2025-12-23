#!/usr/bin/env python3
"""Asset report (cross-platform).

Scans the repo's assets/ folder and prints:
- counts by inferred asset type
- missing bootstrap scene warning
- largest files

No third-party dependencies.
"""

from __future__ import annotations

import argparse
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


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
        # mirror AssetRegistry: if path contains "scenes" segment => Scene
        parts = {p.lower() for p in path.parts}
        return "Scene" if "scenes" in parts else "Other"
    return "Other"


@dataclass(frozen=True)
class AssetItem:
    rel_path: str
    type: str
    size_bytes: int


def iter_files(root: Path) -> Iterable[Path]:
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if p.name.startswith("."):
            continue
        yield p


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=str(Path(__file__).resolve().parents[1]), help="Repo root")
    parser.add_argument("--assets", default="assets", help="Assets dir (relative to repo)")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    assets_dir = (repo / args.assets).resolve()

    if not assets_dir.exists():
        raise SystemExit(f"Assets directory not found: {assets_dir}")

    items: list[AssetItem] = []
    for f in iter_files(assets_dir):
        rel = f.relative_to(assets_dir).as_posix()
        items.append(AssetItem(rel_path=rel, type=classify_asset_type(f), size_bytes=f.stat().st_size))

    print(f"Asset report for: {assets_dir}")

    if not items:
        print("WARNING: No files found under assets/.")

    # Counts
    counts: dict[str, int] = {}
    for it in items:
        counts[it.type] = counts.get(it.type, 0) + 1

    print("\nCounts by type:")
    for k in sorted(counts.keys()):
        print(f"{k:<8} {counts[k]:>6}")

    total_bytes = sum(i.size_bytes for i in items)
    mb = round(total_bytes / (1024 * 1024), 2)
    print(f"Total: {len(items)} files, {mb} MB")

    # Sanity checks
    bootstrap = assets_dir / "scenes" / "bootstrap_scene.json"
    if not bootstrap.exists():
        print("WARNING: Missing bootstrap scene: assets/scenes/bootstrap_scene.json")

    # Uppercase extensions (future case-sensitive tooling)
    upper = [i for i in items if Path(i.rel_path).suffix and Path(i.rel_path).suffix != Path(i.rel_path).suffix.lower()]
    if upper:
        print(f"WARNING: Found {len(upper)} file(s) with uppercase extension.")
        for i in upper[:10]:
            print(f"  {i.rel_path}")

    # Duplicate filenames
    by_name: dict[str, int] = {}
    for i in items:
        name = Path(i.rel_path).name
        by_name[name] = by_name.get(name, 0) + 1
    dupes = sorted(((n, c) for n, c in by_name.items() if c > 1), key=lambda t: t[1], reverse=True)
    if dupes:
        print(f"WARNING: Duplicate filenames detected ({len(dupes)} group(s)).")
        for n, c in dupes[:10]:
            print(f"  {n} x{c}")

    # Largest
    print("\nTop 20 largest files:")
    for i in sorted(items, key=lambda x: x.size_bytes, reverse=True)[:20]:
        print(f"{i.size_bytes:>10}  {i.type:<7}  {i.rel_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
