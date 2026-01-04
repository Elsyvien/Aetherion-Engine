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

---

## Albedo Texture Fix Plan

Albedo textures must be sampled as sRGB while data textures remain linear. The
engine uses `import.srgb` in `.asset.json` to decide the Vulkan texture format.

### Problem
- Albedo/emissive are sRGB color textures.
- Normal/roughness/metallic/AO are linear data textures.
- If all textures are loaded with the same format, gamma is incorrect.

### Fix Steps
1. **Asset Metadata**: Add `import.srgb` to texture `.asset.json` entries:
   ```json
   {
     "id": "...",
     "type": "Texture",
     "import": { "srgb": true }
   }
   ```
2. **Importer Defaults**: Default `srgb = true`, override to `false` for data
   textures (normal/roughness/metallic/AO). `isNormalMap` implies linear.
3. **Vulkan Loader**: Create `VK_FORMAT_*_SRGB` images when `import.srgb` is
   true, otherwise use `VK_FORMAT_*_UNORM`.
4. **Shader Sampling**: No shader changes needed if the format is correct.
5. **Cook Validation**: `cook_assets.py` should warn if `import.srgb` is missing.

### Validation Checklist
- [ ] Texture metadata includes `import.srgb`
- [ ] VulkanViewport selects sRGB vs UNORM correctly
- [ ] cook_assets.py validates `import.srgb`
- [ ] Visual test: compare sRGB vs linear albedo rendering

---

## Virtual Assets (Generative Assets Stub)

Virtual assets are procedurally generated or AI-generated assets that exist in
the registry but have no source file on disk until generated.

### URI Scheme
- `texture://generate/<prompt-hash>` – AI-generated texture
- `mesh://generate/<prompt-hash>` – AI-generated mesh
- `audio://generate/<prompt-hash>` – AI-generated audio clip

### Registry Integration
- `AssetRegistry::RegisterVirtualAsset(uri, type, generator)` adds a virtual entry.
- Virtual assets have a stable GUID derived from the URI hash.
- On first access, the generator is invoked and the result cached to disk.
- Subsequent loads use the cached file.

### Generator Interface (Stub)
```cpp
class IAssetGenerator {
public:
    virtual ~IAssetGenerator() = default;
    virtual bool CanGenerate(const std::string& uri) const = 0;
    virtual std::future<GenerationResult> Generate(
        const std::string& uri,
        const GenerationParams& params) = 0;
};
```

### Editor UX
- Asset Browser shows virtual assets with a "generated" badge.
- Context menu: "Regenerate", "View Prompt", "Export to File".
- Progress indicator during generation.
- Failure state with retry option.

### Implementation Status
- [ ] URI parsing in AssetRegistry
- [ ] IAssetGenerator interface
- [ ] Stub generator returning placeholder assets
- [ ] Disk caching with manifest entries
- [ ] Editor UI integration
