# Hot-Reload System - Datei-Übersicht

## 📁 Neue/Geänderte Dateien

### Core System
```
Engine/Scripting/
├── include/Aetherion/Scripting/
│   ├── GameModule.h              ✨ NEU - Module Interface
│   ├── ModuleLoader.h            ✨ NEU - Dynamic Loading
│   └── BehaviorComponent.h       ✨ NEU - Base Class
└── src/
    ├── ModuleLoader.cpp          ✨ NEU - Implementation
    └── LogicCopilot.cpp          🔧 ERWEITERT - Hot-Reload Integration
```

### Editor
```
Engine/Editor/
├── include/Aetherion/Editor/
│   └── EditorLogicCopilotPanel.h 🔧 ERWEITERT - UI Buttons
└── src/
    └── EditorLogicCopilotPanel.cpp 🔧 ERWEITERT - Hot-Reload Handlers
```

### Build System
```
CMakeLists.txt                    🔧 ERWEITERT - ModuleLoader, dl library
```

### Tools & Scripts
```
tools/
├── build_module.sh               ✨ NEU - Unix Build Script
└── build_module.ps1              ✨ NEU - Windows Build Script
```

### Examples
```
Engine/Scripting/examples/
├── RotationBehavior.h            ✨ NEU - Beispiel Header
└── RotationBehavior.cpp          ✨ NEU - Beispiel Implementation
```

### Documentation
```
docs/
├── hot-reload-system.md          ✨ NEU - System Dokumentation
├── hot-reload-build.md           ✨ NEU - Build Anweisungen
├── HOT_RELOAD_IMPLEMENTATION.md  ✨ NEU - Implementation Details
├── HOT_RELOAD_QUICKSTART.md      ✨ NEU - Quick Start Guide
└── HOT_RELOAD_CHECKLIST.md       ✨ NEU - Integration Checklist

INTEGRATION_COMPLETE.md           ✨ NEU - Integration Summary
README_FILES.md                   ✨ NEU - Diese Datei
```

## 📊 Statistiken

- **Neue Dateien**: 14
- **Geänderte Dateien**: 3
- **Zeilen Code (neu)**: ~1200
- **Zeilen Dokumentation**: ~1500

## 🔑 Schlüssel-Komponenten

### 1. GameModule.h
**Zweck**: Standard-Interface für Module  
**Größe**: ~80 Zeilen  
**Abhängigkeiten**: Core/Types.h, Scene Forward Declarations

```cpp
class IGameModule {
    virtual void OnLoad(Scene::Scene*) = 0;
    virtual void OnUnload() = 0;
    virtual const char* GetModuleName() const = 0;
    virtual uint32_t GetModuleVersion() const = 0;
};
```

### 2. ModuleLoader.h/.cpp
**Zweck**: Dynamic Library Loading/Unloading  
**Größe**: ~300 Zeilen  
**Platform**: Windows (LoadLibrary), Unix (dlopen)

```cpp
class ModuleLoader {
    std::string LoadModule(const std::filesystem::path&);
    bool UnloadModule(const std::string& moduleId);
    bool ReloadModule(const std::string& moduleId);
};
```

### 3. BehaviorComponent.h
**Zweck**: Base Class für runtime-kompilierte Behaviors  
**Größe**: ~35 Zeilen  
**Extends**: Scene::Component

```cpp
class BehaviorComponent : public Scene::Component {
    std::shared_ptr<TransformComponent> GetTransform() const;
};
```

### 4. CodeCompiler::CompileToLibrary()
**Zweck**: Runtime C++ Compilation  
**Größe**: ~150 Zeilen  
**Features**: Auto-Compiler-Detection, Include-Pfade, Linking

```cpp
CompileResult CompileToLibrary(
    const std::vector<std::filesystem::path>& sources,
    const std::filesystem::path& outputLib
);
```

### 5. LogicCopilot Extensions
**Neue Methoden**:
- `CompileToModule()`
- `CompileAndLoadModule()`
- `ReloadModule()`

**Größe**: ~100 Zeilen zusätzlich

### 6. EditorLogicCopilotPanel Extensions
**Neue UI Elemente**:
- "Compile & Load" Button
- "Reload" Button

**Neue Handlers**:
- `OnCompileAndLoadClicked()`
- `OnReloadModuleClicked()`

**Größe**: ~100 Zeilen zusätzlich

## 🔗 Abhängigkeiten

### Neue Dependencies
- `CMAKE_DL_LIBS` (Linux/macOS - für dlopen)

### Compiler Requirements
- C++17 Standard
- g++ / clang++ / MSVC
- Shared Library Support (-shared / -fPIC)

### Runtime Requirements
- AetherionRuntime.dll/.so im Pfad
- Compiler im System-PATH

## 📐 Architektur-Übersicht

```
┌─────────────────┐
│  Editor UI      │
│  (Qt Widgets)   │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│ LogicCopilot    │  ← Natural Language → C++
│ - GenerateCode  │
│ - CompileToLib  │
│ - LoadModule    │
└────────┬────────┘
         │
    ┌────┴────┐
    ↓         ↓
┌──────────┐ ┌──────────────┐
│ Compiler │ │ ModuleLoader │
│ (g++/...) │ │ (dlopen/...) │
└────┬─────┘ └──────┬───────┘
     │              │
     ↓              ↓
┌─────────┐   ┌──────────┐
│ .dll/.so│→  │ IGameMod │
└─────────┘   │   -ule   │
              └────┬─────┘
                   ↓
              ┌──────────┐
              │ Behavior │
              │Component │
              └──────────┘
```

## 🎯 Workflow-Diagramm

```
User Input
    ↓
"Create a rotation behavior"
    ↓
┌─────────────────┐
│ LogicCopilot    │
│ GenerateCode()  │
└────────┬────────┘
         ↓
    Generated C++
    (Header + Source)
         ↓
┌─────────────────┐
│ CodeCompiler    │
│ CompileToLib()  │
└────────┬────────┘
         ↓
    RotationBehavior.dll
         ↓
┌─────────────────┐
│ ModuleLoader    │
│ LoadModule()    │
└────────┬────────┘
         ↓
    IGameModule Instance
         ↓
    OnLoad(scene)
         ↓
    ✅ Behavior läuft!
```

## 🧪 Test-Matrix

| Platform | Compiler | Status |
|----------|----------|--------|
| Windows  | MinGW    | ⏳     |
| Windows  | MSVC     | ⏳     |
| Linux    | GCC      | ⏳     |
| Linux    | Clang    | ⏳     |
| macOS    | Clang    | ⏳     |

## 📈 Performance-Erwartungen

| Operation          | Zeit     |
|--------------------|----------|
| Code Generierung   | 5-10s    |
| Kompilierung       | 2-5s     |
| Module Loading     | <100ms   |
| Hot-Reload (total) | 5-15s    |

## 🔍 Code-Metriken

### Complexity
- **ModuleLoader**: Medium (Plattform-Abstraktion)
- **CompileToLibrary**: Medium (Compiler-Integration)
- **LogicCopilot Extensions**: Low (Wrapper)

### Maintainability
- ✅ Gut dokumentiert
- ✅ Klare Verantwortlichkeiten
- ✅ Platform-abstraktion
- ✅ Error-Handling

### Test Coverage
- ⏳ Unit Tests: Pending
- ⏳ Integration Tests: Pending
- ✅ Example Code: Verfügbar

## 📋 Integration Checklist

- [x] Core System implementiert
- [x] Editor Integration
- [x] Build System aktualisiert
- [x] Build Scripts erstellt
- [x] Beispiele erstellt
- [x] Dokumentation geschrieben
- [ ] Project neu gebaut
- [ ] Tests durchgeführt
- [ ] Performance gemessen

## 🎓 Lern-Ressourcen

### Für Entwickler
1. Start: `HOT_RELOAD_QUICKSTART.md`
2. Deep Dive: `hot-reload-system.md`
3. Build: `hot-reload-build.md`

### Für Architekten
1. Implementation: `HOT_RELOAD_IMPLEMENTATION.md`
2. Checklist: `HOT_RELOAD_CHECKLIST.md`

### API Referenz
- `GameModule.h` - Module Interface
- `ModuleLoader.h` - Loading API
- `BehaviorComponent.h` - Base Class

---

**Erstellt**: 5. Januar 2026  
**Version**: 1.0.0  
**Status**: ✅ Ready for Build
