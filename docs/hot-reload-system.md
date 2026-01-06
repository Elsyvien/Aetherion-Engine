# Hot-Reload Code Compilation System

## Overview

Das Aetherion Engine Hot-Reload System ermöglicht die dynamische Kompilierung und das Laden von C++-Code zur Laufzeit, ohne den Editor neu starten zu müssen. Dies erlaubt einen schnellen Iterationszyklus für die Entwicklung von Behaviors und Gameplay-Logik.

## Architektur

### Komponenten

1. **CodeCompiler** - Kompiliert C++-Code zu Shared Libraries (.dll/.so)
2. **GameModule System** - Standard-Interface für dynamisch geladene Module
3. **ModuleLoader** - Verwaltet das Laden/Entladen von Modulen
4. **BehaviorComponent** - Basisklasse für runtime-kompilierte Behaviors
5. **LogicCopilot Integration** - Verbindet Code-Generierung mit Hot-Reload

### Workflow

```
User Input ("Make this cube spin")
    ↓
LogicCopilot::GenerateCode()
    ↓
Generated C++ Code (Header + Source)
    ↓
CodeCompiler::CompileToLibrary()
    ↓
Shared Library (.dll/.so)
    ↓
ModuleLoader::LoadModule()
    ↓
Module läuft im Editor!
```

## Verwendung

### Im Editor

1. **Code generieren**:
   - Öffne das Logic Copilot Panel
   - Gib einen Prompt ein: "Create a rotation behavior"
   - Klicke auf "Generate"

2. **Kompilieren und Laden**:
   - Nach erfolgreicher Generierung klicke auf "Compile & Load"
   - Das System kompiliert den Code zu einer DLL/SO
   - Das Modul wird automatisch geladen

3. **Hot-Reload**:
   - Bearbeite den generierten Code nach Bedarf
   - Klicke auf "Reload" um das Modul neu zu laden
   - Die Änderungen sind sofort sichtbar!

### Programmierung

#### Eigene Behaviors erstellen

```cpp
#pragma once
#include "Aetherion/Scripting/BehaviorComponent.h"
#include "Aetherion/Scripting/GameModule.h"

namespace Aetherion::Generated
{

class MyBehavior : public Scripting::BehaviorComponent
{
public:
    std::string GetDisplayName() const override { return "My Behavior"; }

protected:
    void OnUpdate(float deltaTime) override
    {
        // Dein Code hier
        auto transform = GetTransform();
        if (transform) {
            // Rotate object
            auto [x, y, z] = transform->GetRotationDegrees();
            transform->SetRotationDegrees(x, y + 45.0f * deltaTime, z);
        }
    }
};

// Module implementation
class MyBehaviorModule : public Scripting::IGameModule
{
public:
    void OnLoad(Scene::Scene* scene) override { /* ... */ }
    void OnUnload() override { /* ... */ }
    const char* GetModuleName() const override { return "MyBehavior"; }
    uint32_t GetModuleVersion() const override { return 1; }
};

} // namespace Aetherion::Generated

// Export functions
extern "C" {
    AETHERION_EXPORT Aetherion::Scripting::IGameModule* CreateGameModule()
    {
        return new Aetherion::Generated::MyBehaviorModule();
    }
    
    AETHERION_EXPORT void DestroyGameModule(Aetherion::Scripting::IGameModule* module)
    {
        delete module;
    }
}
```

#### Programmatisches Laden

```cpp
// LogicCopilot-Instanz erstellen
auto copilot = std::make_unique<Scripting::LogicCopilot>();
copilot->SetProjectRoot(projectPath);
copilot->SetOutputDirectory(outputPath);

// Code generieren
Scripting::CodeGenerationRequest request;
request.prompt = "Create a rotation behavior";
request.systemType = "Behavior";
request.autoCompile = false;

auto result = copilot->GenerateCodeSync(request);

// Zu Modul kompilieren und laden
std::string moduleId;
std::vector<std::string> errors;
bool success = copilot->CompileAndLoadModule(result.code, scene, moduleId, errors);

if (success) {
    std::cout << "Module loaded: " << moduleId << std::endl;
}

// Später: Hot-Reload
copilot->ReloadModule(moduleId);
```

## Compiler-Unterstützung

### Windows
- **MinGW**: Standard-Compiler (g++)
- **MSVC**: Wird automatisch erkannt wenn Visual Studio installiert ist

### Linux/macOS
- **GCC**: Standard
- **Clang**: Wird automatisch erkannt

## Include-Pfade

Das System fügt automatisch folgende Include-Pfade hinzu:
- `Engine/` - Engine-Header
- Qt6-Verzeichnisse (falls verfügbar)

## Linking

Kompilierte Module werden automatisch gelinkt gegen:
- `AetherionRuntime` - Engine-Core-Library
- Qt6-Libraries (falls benötigt)

## Beispiel-Workflow

### "Make this cube spin"

1. **User Input**: "Make this cube spin"

2. **Generated Header** (`RotationBehavior.h`):
```cpp
#pragma once
#include "Aetherion/Scripting/BehaviorComponent.h"
#include "Aetherion/Scripting/GameModule.h"

namespace Aetherion::Generated {
    class RotationBehavior : public Scripting::BehaviorComponent {
        // ...
    };
    
    class RotationBehaviorModule : public Scripting::IGameModule {
        // ...
    };
}

extern "C" {
    AETHERION_EXPORT Aetherion::Scripting::IGameModule* CreateGameModule();
    AETHERION_EXPORT void DestroyGameModule(Aetherion::Scripting::IGameModule*);
}
```

3. **Generated Source** (`RotationBehavior.cpp`):
```cpp
#include "RotationBehavior.h"
#include "Aetherion/Scene/Entity.h"

namespace Aetherion::Generated {
    void RotationBehavior::OnUpdate(float deltaTime) {
        auto transform = GetTransform();
        if (!transform) return;
        
        auto [x, y, z] = transform->GetRotationDegrees();
        y += 45.0f * deltaTime;
        transform->SetRotationDegrees(x, y, z);
    }
}

extern "C" {
    AETHERION_EXPORT Aetherion::Scripting::IGameModule* CreateGameModule() {
        return new Aetherion::Generated::RotationBehaviorModule();
    }
    // ...
}
```

4. **Compilation**:
```bash
g++ -shared -std=c++17 -O2 \
    -I"Engine/" \
    RotationBehavior.cpp \
    -o modules/RotationBehavior.dll \
    -L"build-mingw/" -lAetherionRuntime
```

5. **Loading**:
- Modul wird geladen
- `CreateGameModule()` wird aufgerufen
- `OnLoad(scene)` wird ausgeführt

6. **Execution**:
- `RotationBehavior::OnUpdate()` wird jeden Frame aufgerufen
- Cube rotiert!

## Technische Details

### Memory Management
- Module werden via `dlopen`/`LoadLibrary` geladen
- Instanzen werden via Factory-Funktionen erstellt
- Proper Cleanup beim Entladen

### Thread Safety
- ModuleLoader ist nicht thread-safe
- Alle Operationen sollten im Main-Thread erfolgen

### Symbol Resolution
- `extern "C"` verhindert Name-Mangling
- Standard Entry-Points: `CreateGameModule`, `DestroyGameModule`

### Platform-Spezifika

#### Windows
```cpp
#define AETHERION_EXPORT __declspec(dllexport)
void* handle = LoadLibraryW(path);
auto fn = GetProcAddress(handle, "CreateGameModule");
FreeLibrary(handle);
```

#### Linux/macOS
```cpp
#define AETHERION_EXPORT __attribute__((visibility("default")))
void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
auto fn = dlsym(handle, "CreateGameModule");
dlclose(handle);
```

## Fehlerbehandlung

### Compilation Errors
- Werden in `CompileResult::errors` zurückgegeben
- Enthalten Compiler-Output mit Zeilennummern

### Loading Errors
- `LoadModule()` gibt leeren String zurück
- Fehlermeldungen werden auf stderr ausgegeben

### Runtime Errors
- Module sollten keine Exceptions aus `OnUpdate()` werfen
- Crashes führen zum Absturz des gesamten Editors!

## Best Practices

1. **Testen vor dem Laden**: Nutze `TestCompile()` für Syntax-Checks
2. **Versioning**: Inkrementiere `GetModuleVersion()` bei Änderungen
3. **Cleanup**: Immer `OnUnload()` implementieren
4. **Error Handling**: Defensive Programmierung in `OnUpdate()`
5. **Performance**: Minimale Arbeit in `OnUpdate()` (wird jeden Frame aufgerufen)

## Limitierungen

- **Windows**: Modules können nicht überschrieben werden während sie geladen sind
- **Template-Code**: Komplexe Templates können Kompilierprobleme verursachen
- **Debugging**: Debug-Symbole müssen separat behandelt werden
- **Dependencies**: Externe Libraries müssen manuell gelinkt werden

## Zukünftige Erweiterungen

- [ ] Automatisches Neucompilieren bei Dateiänderungen (File Watcher)
- [ ] Debug-Symbol-Unterstützung für Hot-Reload
- [ ] Intellisense/Autocomplete für generierte Behaviors
- [ ] Visual Scripting-Integration
- [ ] Module Dependency Graph
- [ ] Incremental Compilation
- [ ] Cloud-basierte Kompilierung

## Siehe auch

- [Engine Scripting](scripting.md)
- [Logic Copilot](logic-copilot.md)
- [Component System](scene.md)
