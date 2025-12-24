# Asset Pipeline Notes

## Asset-ID policy
- Jede Asset-Datei bekommt eine stabile GUID in einer Nachbar-Datei `<asset>.asset.json`.
- Der Editor/AssetRegistry nutzt die GUID als Asset-ID (nicht mehr den Pfad).
- Beim Rename/Move im Editor wird die `.asset.json` mitgezogen, die GUID bleibt stabil.
- Die Anzeige im Asset Browser zeigt weiterhin den relativen Pfad, damit die UI lesbar bleibt.

## Hot reload behavior
- Der Editor pollt das Assets-Verzeichnis (alle 500ms) und ruft `AssetRegistry::Rescan`.
- Bei Aenderungen:
  - Asset Browser wird neu aufgebaut.
  - Inspector wird fuer die aktuell selektierte Asset-ID neu aufgebaut.
  - Mesh Preview aktualisiert sich automatisch oder leert sich, wenn das Asset geloescht wurde.
  - GPU-Caches (Meshes/Textures) werden gezielt invalidiert.
- Die Scene bleibt stabil, alte Asset-IDs bleiben gueltig (GUID).

## How to cook (minimal)
- Ziel: Trennung zwischen Editor-Rohdaten und Runtime-Output.
- `tools/cook_assets.py` kopiert Assets in einen Ausgabeordner und schreibt eine Manifest-Datei:

```bash
python3 tools/cook_assets.py --assets assets --out build/cooked
```

- Output: `build/cooked/asset_index.json` mit `{ id, path, type }`.
