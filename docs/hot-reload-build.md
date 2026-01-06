# Hot-Reload System Build Instructions

## Neue Dateien zum CMakeLists.txt hinzufügen

Füge folgende Dateien zu `Engine/Scripting/CMakeLists.txt` hinzu:

```cmake
# Scripting Module - Hot-Reload System
set(SCRIPTING_SOURCES
    # ... existing sources ...
    
    # Hot-Reload System
    src/ModuleLoader.cpp
    
    # Examples (optional - nur für Testing)
    examples/RotationBehavior.cpp
)

set(SCRIPTING_HEADERS
    # ... existing headers ...
    
    # Hot-Reload System
    include/Aetherion/Scripting/GameModule.h
    include/Aetherion/Scripting/ModuleLoader.h
    include/Aetherion/Scripting/BehaviorComponent.h
    
    # Examples
    examples/RotationBehavior.h
)
```

## Platform-spezifische Linking

```cmake
# Link platform-specific dynamic loading libraries
if(WIN32)
    # Windows: No additional libs needed (LoadLibrary is in kernel32)
else()
    # Linux/macOS: Need dl library
    target_link_libraries(AetherionRuntime PRIVATE dl)
endif()
```

## Test-Modul kompilieren (manuell)

### Windows (MinGW):
```bash
cd Engine/Scripting/examples
g++ -shared -std=c++17 -O2 ^
    -I../../ ^
    -I../../../ThirdParty/glm ^
    RotationBehavior.cpp ^
    -o RotationBehavior.dll ^
    -L../../../build-mingw ^
    -lAetherionRuntime

# Teste das Laden
cd ../../../build-mingw
./AetherionEditor.exe
```

### Linux:
```bash
cd Engine/Scripting/examples
g++ -shared -std=c++17 -O2 -fPIC \
    -I../../ \
    -I../../../ThirdParty/glm \
    RotationBehavior.cpp \
    -o RotationBehavior.so \
    -L../../../build \
    -lAetherionRuntime \
    -Wl,-rpath,'$ORIGIN'

cd ../../../build
./AetherionEditor
```

## Automatisches Build-Skript

Erstelle `tools/build_module.sh`:

```bash
#!/bin/bash
# Build a hot-reload module

MODULE_NAME=$1
MODULE_DIR="Engine/Scripting/examples"
BUILD_DIR="build-mingw"

if [ -z "$MODULE_NAME" ]; then
    echo "Usage: ./build_module.sh <ModuleName>"
    exit 1
fi

echo "Building module: $MODULE_NAME"

# Determine platform
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    # Windows
    g++ -shared -std=c++17 -O2 \
        -I"Engine/" \
        "$MODULE_DIR/${MODULE_NAME}.cpp" \
        -o "$BUILD_DIR/modules/${MODULE_NAME}.dll" \
        -L"$BUILD_DIR" \
        -lAetherionRuntime
else
    # Linux/macOS
    g++ -shared -std=c++17 -O2 -fPIC \
        -I"Engine/" \
        "$MODULE_DIR/${MODULE_NAME}.cpp" \
        -o "$BUILD_DIR/modules/${MODULE_NAME}.so" \
        -L"$BUILD_DIR" \
        -lAetherionRuntime \
        -Wl,-rpath,'$ORIGIN'
fi

if [ $? -eq 0 ]; then
    echo "✓ Module built successfully"
else
    echo "✗ Build failed"
    exit 1
fi
```

Verwendung:
```bash
chmod +x tools/build_module.sh
./tools/build_module.sh RotationBehavior
```

## Windows PowerShell Version

`tools/build_module.ps1`:

```powershell
param(
    [Parameter(Mandatory=$true)]
    [string]$ModuleName
)

$ModuleDir = "Engine/Scripting/examples"
$BuildDir = "build-mingw"
$OutputDir = "$BuildDir/modules"

Write-Host "Building module: $ModuleName"

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Compile
$env:PATH = "C:/Qt/Tools/mingw1310_64/bin;$env:PATH"

g++ -shared -std=c++17 -O2 `
    -I"Engine/" `
    "$ModuleDir/$ModuleName.cpp" `
    -o "$OutputDir/$ModuleName.dll" `
    -L"$BuildDir" `
    -lAetherionRuntime

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Module built successfully" -ForegroundColor Green
} else {
    Write-Host "✗ Build failed" -ForegroundColor Red
    exit 1
}
```

Verwendung:
```powershell
.\tools\build_module.ps1 -ModuleName RotationBehavior
```

## Integration in Editor

Das System ist automatisch integriert:

1. Logic Copilot Panel öffnen
2. Code generieren lassen
3. "Compile & Load" Button klicken
4. Modul läuft sofort!

Keine manuelle Kompilierung notwendig - alles wird automatisch erledigt.
