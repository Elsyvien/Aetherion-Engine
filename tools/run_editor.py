#!/usr/bin/env python3
"""Run AetherionEditor with convenient env wiring (cross-platform).

This intentionally mirrors the VS Code tasks defaults but allows overrides.

Examples:
  python tools/run_editor.py
  python tools/run_editor.py --build-dir build-mingw --exe AetherionEditor.exe
  python tools/run_editor.py --log tools/logs/editor.log

On Windows/MinGW you'll likely want to pass Qt paths (or rely on defaults):
  python tools/run_editor.py --qt-tools-bin C:/Qt/Tools/mingw1310_64/bin --qt-bin C:/Qt/6.9.1/mingw_64/bin --qt-plugin-path C:/Qt/6.9.1/mingw_64/plugins

On macOS (Homebrew Qt), typical values are something like:
  --qt-bin "$(brew --prefix qt)/bin" --qt-plugin-path "$(brew --prefix qt)/share/qt/plugins"

No third-party dependencies.
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


def prepend_path(env: dict[str, str], key: str, prefix: str) -> None:
    if not prefix:
        return
    sep = os.pathsep
    current = env.get(key, "")
    env[key] = prefix + (sep + current if current else "")


def main() -> int:
    default_repo = Path(__file__).resolve().parents[1]

    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=str(default_repo), help="Repo root")
    parser.add_argument("--build-dir", default="build-mingw", help="Build directory under repo")
    parser.add_argument("--exe", default="AetherionEditor.exe" if os.name == "nt" else "AetherionEditor", help="Executable name")
    parser.add_argument("--cwd", default=None, help="Working dir (default: build dir)")

    parser.add_argument("--qt-tools-bin", default="C:/Qt/Tools/mingw1310_64/bin" if os.name == "nt" else "", help="Qt toolchain bin to prepend to PATH")
    parser.add_argument("--qt-bin", default="C:/Qt/6.9.1/mingw_64/bin" if os.name == "nt" else "", help="Qt runtime bin to prepend to PATH")
    parser.add_argument("--qt-plugin-path", default="C:/Qt/6.9.1/mingw_64/plugins" if os.name == "nt" else "", help="QT_PLUGIN_PATH")

    parser.add_argument("--log", default=None, help="If set, tee stdout+stderr to this file")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    build_dir = (repo / args.build_dir).resolve()
    exe_path = (build_dir / args.exe).resolve()
    cwd = Path(args.cwd).resolve() if args.cwd else build_dir

    if not exe_path.exists():
        raise SystemExit(f"Executable not found: {exe_path}. Build first.")

    env = dict(os.environ)
    if args.qt_tools_bin:
        prepend_path(env, "PATH", args.qt_tools_bin)
    if args.qt_bin:
        prepend_path(env, "PATH", args.qt_bin)
    if args.qt_plugin_path:
        env["QT_PLUGIN_PATH"] = args.qt_plugin_path

    if args.log:
        log_path = Path(args.log)
        log_path.parent.mkdir(parents=True, exist_ok=True)

        with log_path.open("w", encoding="utf-8", errors="replace") as f:
            f.write(f"# cwd: {cwd}\n")
            f.write(f"# exe: {exe_path}\n")
            f.flush()

            proc = subprocess.Popen([str(exe_path)], cwd=str(cwd), env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            assert proc.stdout is not None
            for line in proc.stdout:
                print(line, end="")
                f.write(line)
            return proc.wait()

    return subprocess.call([str(exe_path)], cwd=str(cwd), env=env)


if __name__ == "__main__":
    raise SystemExit(main())
