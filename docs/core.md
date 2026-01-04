# Core Utilities

## Types and Versioning
File: `Engine/Core/include/Aetherion/Core/Types.h`

- Version
  - Simple struct with major/minor/patch and `ToString()`.
- EntityId / ComponentId
  - 64-bit IDs with `kInvalidEntityId` and `kInvalidComponentId` set to 0.
- NonCopyable
  - Base class to disable copy/assign.
- EnginePaths
  - Holds `root`, `content`, and `cache` paths (placeholder for VFS).

## Logging
File: `Engine/Core/include/Aetherion/Core/Types.h`
File: `Engine/Core/src/Core.cpp`

- Log::Print routes to stdout/stderr with `[INFO]`, `[WARN]`, `[ERROR]`, `[DEBUG]`.
- Log::AddListener registers callbacks that receive each log entry.
- InitializeCoreModule exists as a placeholder for future core bootstrap.

## String Helpers
File: `Engine/Core/include/Aetherion/Core/String.h`

- ToLower: returns a lowercased copy.
- HasSuffix: suffix check.
- ContainsCaseInsensitive: case-insensitive substring search.

## Math Helpers
File: `Engine/Core/include/Aetherion/Core/Math.h`

All functions operate on raw `float[16]` matrices and `float[3]` vectors.
Matrices are treated as column-major (indices are `col * 4 + row`).

- Mat4Identity, Mat4Mul
- Mat4RotationX/Y/Z, Mat4Translation, Mat4Scale
- Mat4Compose: builds a TRS matrix from translation, Euler radians, and scale.
- Vec3Normalize, Vec3Cross, Vec3Dot

## UUID
File: `Engine/Core/include/Aetherion/Core/UUID.h`
File: `Engine/Core/src/UUID.cpp`

- GenerateUUID() creates a 36-char GUID-like string (hex + hyphens).
