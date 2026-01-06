# Hot-Reload System - Quick Start Guide

## 🚀 Schnellstart

### 1. Projekt neu bauen

```bash
# Windows (PowerShell)
cmake --build build-mingw --target AetherionEditor

# Linux/macOS
cmake --build build --target AetherionEditor
```

### 2. Beispiel-Modul testen

#### Option A: Manueller Build (zum Testen)

**Windows:**
```powershell
.\tools\build_module.ps1 -ModuleName RotationBehavior
```

**Linux/macOS:**
```bash
chmod +x tools/build_module.sh
./tools/build_module.sh RotationBehavior
```

#### Option B: Im Editor (empfohlen)

1. **Editor starten:**
   ```bash
   # Windows
   .\build-mingw\AetherionEditor.exe
   
   # Linux/macOS
   ./build/AetherionEditor
   ```

2. **Logic Copilot öffnen:**
   - Menü: `Window` → `Logic Copilot`
   - Oder: `Ctrl+Shift+L`

3. **Code generieren:**
   - Prompt eingeben: "Create a rotation behavior that spins an entity around the Y-axis"
   - System Type: `Behavior`
   - Klick auf `Generate`

4. **Kompilieren und Laden:**
   - Nach erfolgreicher Generierung
   - Klick auf `Compile & Load`
   - ✅ Modul läuft jetzt!

5. **Behavior testen:**
   - Wähle ein Entity in der Scene
   - Das Behavior wird automatisch angewendet
   - Entity sollte nun rotieren

## 📝 Verwendung im Code

### Modul programmatisch laden

```cpp
#include "Aetherion/Scripting/LogicCopilot.h"
#include "Aetherion/Scene/Scene.h"

// Setup
auto copilot = std::make_unique<Scripting::LogicCopilot>();
copilot->SetProjectRoot(projectPath);
copilot->SetOutputDirectory(outputPath);

// Generiere Code
Scripting::CodeGenerationRequest request;
request.prompt = "Create a rotation behavior";
request.systemType = "Behavior";

auto result = copilot->GenerateCodeSync(request);

// Kompiliere und lade
std::string moduleId;
std::vector<std::string> errors;
bool success = copilot->CompileAndLoadModule(
    result.code, 
    scene, 
    moduleId, 
    errors
);

if (success) {
    std::cout << "✓ Module loaded: " << moduleId << std::endl;
} else {
    for (const auto& err : errors) {
        std::cerr << "✗ " << err << std::endl;
    }
}
```

### ModuleLoader direkt verwenden

```cpp
#include "Aetherion/Scripting/ModuleLoader.h"

auto loader = std::make_unique<Scripting::ModuleLoader>();
loader->SetSceneContext(scene);

// Lade Modul
std::string moduleId = loader->LoadModule("build-mingw/modules/RotationBehavior.dll");

if (!moduleId.empty()) {
    std::cout << "✓ Module loaded: " << moduleId << std::endl;
    
    // Hole Module-Instanz
    auto* module = loader->GetModule(moduleId);
    if (module) {
        std::cout << "  Name: " << module->GetModuleName() << std::endl;
        std::cout << "  Version: " << module->GetModuleVersion() << std::endl;
    }
    
    // Später: Hot-Reload
    bool reloaded = loader->ReloadModule(moduleId);
    if (reloaded) {
        std::cout << "✓ Module reloaded" << std::endl;
    }
}
```

## 🎯 Workflow-Beispiel: "Make this cube spin"

### Schritt 1: Code generieren

**Eingabe:**
```
Create a behavior that makes an entity rotate continuously around the Y-axis at 45 degrees per second
```

**Generierter Code:**
```cpp
// RotationBehavior.h
namespace Aetherion::Generated {
    class RotationBehavior : public Scripting::BehaviorComponent {
        void OnUpdate(float deltaTime) override {
            auto transform = GetTransform();
            if (!transform) return;
            
            auto [x, y, z] = transform->GetRotationDegrees();
            y += 45.0f * deltaTime;
            transform->SetRotationDegrees(x, y, z);
        }
    };
}
```

### Schritt 2: Kompilieren

**Automatisch:** Klick auf "Compile & Load"

**Manuell:**
```bash
g++ -shared -std=c++17 -O2 \
    -I"Engine/" \
    RotationBehavior.cpp \
    -o modules/RotationBehavior.dll \
    -L"build-mingw/" \
    -lAetherionRuntime
```

### Schritt 3: Laden

**Im Editor:** Automatisch nach Kompilierung

**Programmatisch:**
```cpp
std::string moduleId = loader->LoadModule("modules/RotationBehavior.dll");
```

### Schritt 4: Verwenden

Das Behavior läuft automatisch! Die `OnUpdate()` Methode wird jeden Frame aufgerufen.

## 🔄 Hot-Reload Workflow

1. **Generiere und lade ein Modul** (siehe oben)

2. **Bearbeite den generierten Code:**
   - Öffne die .cpp Datei
   - Ändere z.B. die Rotationsgeschwindigkeit von 45° auf 90°
   ```cpp
   y += 90.0f * deltaTime;  // Schneller!
   ```

3. **Recompile:**
   ```bash
   # Manuell
   .\tools\build_module.ps1 -ModuleName RotationBehavior
   
   # Oder im Editor
   # (Code wird automatisch neu kompiliert bei "Reload")
   ```

4. **Reload im Editor:**
   - Klick auf `Reload` Button
   - ✅ Änderungen sind sofort sichtbar!

## 🛠️ Troubleshooting

### Compilation Error: "g++ not found"

**Windows:**
```powershell
# Qt MinGW PATH hinzufügen
$env:PATH = "C:/Qt/Tools/mingw1310_64/bin;$env:PATH"
```

**Linux:**
```bash
sudo apt install build-essential
```

**macOS:**
```bash
xcode-select --install
```

### Error: "AetherionRuntime not found"

Stelle sicher, dass das Projekt gebaut wurde:
```bash
cmake --build build-mingw --target AetherionRuntime
```

### Module lädt nicht

1. **Prüfe Pfad:**
   ```cpp
   // Absoluter Pfad verwenden
   loader->LoadModule("C:/full/path/to/module.dll");
   ```

2. **Prüfe Dependencies:**
   - AetherionRuntime.dll muss im gleichen Verzeichnis sein
   - Oder in einem Verzeichnis im PATH

3. **Prüfe Export-Funktionen:**
   ```cpp
   // Müssen vorhanden sein:
   extern "C" {
       AETHERION_EXPORT IGameModule* CreateGameModule();
       AETHERION_EXPORT void DestroyGameModule(IGameModule*);
   }
   ```

### Runtime Crash

**Häufige Ursachen:**
1. Exception in `OnUpdate()` → Immer try-catch verwenden
2. Null-Pointer → Immer prüfen: `if (!transform) return;`
3. Stack Overflow → Keine Rekursion in `OnUpdate()`

**Debug:**
```cpp
void OnUpdate(float deltaTime) override {
    try {
        auto transform = GetTransform();
        if (!transform) {
            std::cerr << "No transform!" << std::endl;
            return;
        }
        // ... rest of code
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

## 📚 Weitere Ressourcen

- [Hot-Reload System Dokumentation](hot-reload-system.md)
- [Build-Anweisungen](hot-reload-build.md)
- [Implementation Details](HOT_RELOAD_IMPLEMENTATION.md)

## 🎉 Fertig!

Du kannst jetzt:
- ✅ Code mit Natural Language generieren
- ✅ Zur Runtime kompilieren
- ✅ Sofort im Editor laden
- ✅ Hot-Reload ohne Neustart
- ✅ Iterieren in Sekunden statt Minuten!

**Happy Coding! 🚀**
