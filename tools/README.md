# Tools (misc)

Kleine Helfer-Skripte, die unabhängig vom Engine-Code sind.

## Cross-platform (Python)

### Editor starten

```bash
python3 tools/run_editor.py
```

Optional mit Log:

```bash
python3 tools/run_editor.py --log tools/logs/editor.log
```

### Asset-Report

```bash
python3 tools/asset_report.py
```

### Assets cooken (minimaler Runtime-Output)

```bash
python3 tools/cook_assets.py --assets assets --out build/cooked
```

## macOS/Linux (Bash Wrapper)

```bash
./tools/run-editor.sh
./tools/asset-report.sh
```

## Windows / PowerShell

### Editor starten (MinGW, mit Qt-PATH)

```powershell
./tools/run-editor-mingw.ps1
```

### Editor starten + Log in Datei schreiben

```powershell
./tools/run-editor-mingw-log.ps1
```

Logfiles landen unter `tools/logs/`.

### Asset-Report (Scan von `assets/`)

```powershell
./tools/asset-report.ps1
```

Gibt Counts pro Kategorie aus und warnt bei typischen Problemen (fehlende Bootstrap-Scene, leere Ordner, etc.).

## Hinweise

- Die Skripte verwenden bewusst nur relative Pfade ab Repo-Root.
- Standardwerte (Qt-Pfade) sind an die VS-Code Tasks angelehnt und können als Parameter überschrieben werden.
