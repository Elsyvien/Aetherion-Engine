# Assets and Materials

## AssetRegistry
Files:
- `Engine/Assets/include/Aetherion/Assets/AssetRegistry.h`
- `Engine/Assets/src/AssetRegistry.cpp`

Key responsibilities:
- Scan/rescan an assets root, classify file types, and build an entry list.
- Maintain stable asset IDs via per-asset `.asset.json` metadata files.
- Track asset changes with a serial log for editor polling.
- Import glTF meshes and extract mesh/texture/material dependencies.

### Asset Types and Extensions
Classification is based on file extension:
- Texture: .png .jpg .jpeg .tga .bmp .gif .dds .ktx .ktx2
- Mesh: .gltf .glb .obj .fbx .dae
- Audio: .wav .mp3 .ogg .flac .aiff
- Script: .lua .py .js .cs
- Shader: .vert .frag .glsl .spv
- Scene: .json (only when under a `scenes` folder)
- Other: everything else (including `.mat` materials)

### Metadata Files
For an asset at `foo.png`, the registry expects a sibling `foo.png.asset.json`.
Metadata stores:
- `version`, `id`, `type`, `source`
- `import` settings for meshes and textures

Mesh import settings:
- scale, centerMesh, generateNormals, generateTangents,
  flipUVs, flipWinding, optimize

Texture import settings:
- srgb, generateMipmaps, flipVertical, isNormalMap

### Change Tracking
AssetRegistry emits changes into a bounded log and increments a serial number:
- Added, Modified, Removed, Moved, Metadata

Editor polls `GetChangesSince(serial)` to update UI and GPU caches.

### Cached Data
- MeshData: positions/normals/colors/uvs/tangents, indices, bounds.
- CachedMesh: source path + referenced textures/materials.
- CachedTexture: id + path.
- Material records (see below).

## Material
File: `Engine/Assets/include/Aetherion/Assets/Material.h`

Material holds PBR-like values:
- base color (RGBA)
- metallic, roughness
- emissive factor
- texture asset IDs (albedo, normal, metallic-roughness, emissive, occlusion)

Extension: `.mat`

AssetRegistry exposes:
- CreateMaterial(name)
- SaveMaterial(assetId)
- GetMaterial(id)

Note: materials are treated as a special-case asset (AssetType::Other).

## glTF Import
AssetRegistry::ImportGltf:
- Uses cgltf to parse meshes and materials.
- Produces cached mesh/material/texture IDs.
- Writes/updates metadata and dependency lists.
- Supports reimport via ReimportMeshAsset().

See `docs/asset-pipeline.md` for hot-reload and cooking notes.
