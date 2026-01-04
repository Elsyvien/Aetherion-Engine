# Quick-Start Implementation Guide

## 5-Minute Overview

The Aetherion Editor UI has been completely redesigned with all necessary components created and documented. You have everything you need to implement it.

## What You Have

✅ **TabPanelManager.h** - The core tabbed panel system (150 lines)
✅ **TabPanelManager.cpp** - Complete implementation (230 lines)
✅ **CMakeLists.txt** - Updated with new files
✅ **5 Comprehensive Documentation Files**

## What You Need to Do

### Phase 1: Basic Integration (30 minutes)

1. **Add to EditorMainWindow.h**
   ```cpp
   class TabPanelManager* m_panelManager = nullptr;
   ```

2. **In Constructor, Replace:**
   ```cpp
   // OLD:
   auto *centerSplit = new QSplitter(Qt::Horizontal, this);
   // ... 3500+ lines of dock setup
   
   // NEW:
   m_panelManager = new TabPanelManager(this);
   setCentralWidget(m_panelManager);
   m_panelManager->CreatePanelGroups(this);
   ```

3. **Replace CreateDockPanels() method:**
   Copy the entire new `CreateTabPanels()` implementation from INTEGRATION_CODE_SNIPPETS.md (about 80 lines)

4. **Update View Menu:**
   Replace all dock visibility actions with tab switching (see INTEGRATION_CODE_SNIPPETS.md)

### Phase 2: Compilation (10 minutes)

```bash
cd build-mingw
cmake --build . --clean-first -- -j 8
```

### Phase 3: Testing (15 minutes)

- Click tabs to switch panels
- Try keyboard shortcuts (Ctrl+H, Ctrl+I, etc.)
- Drag splitters to resize
- Press Ctrl+` to toggle bottom panel
- Close and restart - layout should persist

## Common Issues & Solutions

### Issue: "TabPanelManager not found"
**Solution**: Check that the #include is correct:
```cpp
#include "Aetherion/Editor/TabPanelManager.h"
```

### Issue: "Splitter doesn't resize"
**Solution**: Ensure you're dragging the splitter handle (4px wide), not the panel background.

### Issue: "Some panels not showing"
**Solution**: Check that all panels are added to the manager in CreateTabPanels():
```cpp
m_panelManager->AddToLeftPanel(m_hierarchyPanel, tr("Hierarchy"));
// ... etc for all panels
```

### Issue: "Tabs aren't switching"
**Solution**: Verify menu actions call SetActiveTab:
```cpp
m_panelManager->SetActiveLeftTab(0);
m_panelManager->SetActiveRightTab(0);
m_panelManager->SetActiveBottomTab(0);
```

## File-by-File Changes

### EditorMainWindow.h
- Remove all `QDockWidget*` member variables (12 lines)
- Remove all `QAction*` visibility actions (13 lines)
- Add `TabPanelManager* m_panelManager = nullptr;` (1 line)
- Change `CreateDockPanels()` to `CreateTabPanels()` (method signature)

### EditorMainWindow.cpp
- Replace viewport/dock creation (lines ~520-600)
- Replace entire CreateDockPanels() method (lines ~3490-3750)
- Replace View menu setup (lines ~1200-1300)
- Update SaveLayout() and RestoreLayout() methods
- Update any method that calls ShowHierarchy(), ShowInspector(), etc.

### CMakeLists.txt
- Add `Engine/Editor/src/TabPanelManager.cpp` to EDITOR_SOURCES

## Detailed Code Changes

### Change 1: Include the Header
```cpp
// In EditorMainWindow.cpp at the top with other includes:
#include "Aetherion/Editor/TabPanelManager.h"
```

### Change 2: Initialize in Constructor
Find the constructor initialization and add:
```cpp
// After creating selection and command palette
m_panelManager = new TabPanelManager(this);
setCentralWidget(m_panelManager);
m_panelManager->CreatePanelGroups(this);
```

### Change 3: Create Panels
Replace the entire `CreateDockPanels()` implementation:
```cpp
void EditorMainWindow::CreateTabPanels()
{
    if (!m_panelManager) return;
    
    // Left Panel
    m_hierarchyPanel = new EditorHierarchyPanel(m_panelManager);
    m_panelManager->AddToLeftPanel(m_hierarchyPanel, tr("Hierarchy"));
    
    m_assetBrowser = new EditorAssetBrowser(m_panelManager);
    m_panelManager->AddToLeftPanel(m_assetBrowser, tr("Assets"));
    
    // Right Panel
    m_inspectorPanel = new EditorInspectorPanel(m_panelManager);
    m_inspectorPanel->SetCommandExecutor(
        [this](std::unique_ptr<Command> cmd) { ExecuteCommand(std::move(cmd)); });
    m_panelManager->AddToRightPanel(m_inspectorPanel, tr("Inspector"));
    
    m_meshPreview = new EditorMeshPreview(m_panelManager);
    m_panelManager->AddToRightPanel(m_meshPreview, tr("Mesh Preview"));
    
    m_cameraPreview = new EditorCameraPreview(m_panelManager);
    m_panelManager->AddToRightPanel(m_cameraPreview, tr("Camera Preview"));
    
    m_copilotPanel = new AICopilotPanel(m_panelManager);
    m_panelManager->AddToRightPanel(m_copilotPanel, tr("AI Copilot"));
    
    connect(m_copilotPanel, &AICopilotPanel::PromptSubmitted, this,
            [this](const QString &prompt) {
                m_copilotPanel->SetProcessing(true);
                HandleCopilotPrompt(prompt);
                m_copilotPanel->SetProcessing(false);
            });
    
    // Bottom Panel
    m_console = new EditorConsole(m_panelManager);
    m_panelManager->AddToBottomPanel(m_console, tr("Console"));
    
    m_statsPanel = new EditorStatisticsPanel(m_panelManager);
    m_panelManager->AddToBottomPanel(m_statsPanel, tr("Statistics"));
    
    m_animationPanel = new EditorAnimationPanel(m_panelManager);
    m_panelManager->AddToBottomPanel(m_animationPanel, tr("Animation"));
    
    m_logicCopilotPanel = new EditorLogicCopilotPanel(m_panelManager);
    m_panelManager->AddToBottomPanel(m_logicCopilotPanel, tr("Logic Copilot"));
    
    m_assetGenPanel = new EditorAssetGenerationPanel(m_panelManager);
    m_panelManager->AddToBottomPanel(m_assetGenPanel, tr("Asset Generator"));
}
```

### Change 4: Update View Menu
Replace the dock visibility actions with:
```cpp
auto *viewMenu = menuBar()->addMenu(tr("&View"));

// Left Panel
auto *showHierarchyAction = viewMenu->addAction(tr("Hierarchy\tCtrl+H"));
connect(showHierarchyAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveLeftTab(0);
});

auto *showAssetsAction = viewMenu->addAction(tr("Asset Browser\tCtrl+Shift+A"));
connect(showAssetsAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveLeftTab(1);
});

viewMenu->addSeparator();

// Right Panel
auto *showInspectorAction = viewMenu->addAction(tr("Inspector\tCtrl+I"));
connect(showInspectorAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveRightTab(0);
});

auto *showMeshPreviewAction = viewMenu->addAction(tr("Mesh Preview\tCtrl+Shift+M"));
connect(showMeshPreviewAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveRightTab(1);
});

auto *showCameraPreviewAction = viewMenu->addAction(tr("Camera Preview\tCtrl+Shift+C"));
connect(showCameraPreviewAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveRightTab(2);
});

auto *showAICopilotAction = viewMenu->addAction(tr("AI Copilot\tCtrl+Shift+P"));
connect(showAICopilotAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) m_panelManager->SetActiveRightTab(3);
});

viewMenu->addSeparator();

// Bottom Panel
auto *toggleBottomPanelAction = viewMenu->addAction(tr("Toggle Bottom Panel\tCtrl+`"));
connect(toggleBottomPanelAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetBottomPanelVisible(!m_panelManager->IsBottomPanelVisible());
    }
});

auto *showConsoleAction = viewMenu->addAction(tr("Console\tCtrl+Shift+`"));
connect(showConsoleAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetBottomPanelVisible(true);
        m_panelManager->SetActiveBottomTab(0);
    }
});

auto *showStatsAction = viewMenu->addAction(tr("Statistics\tCtrl+Shift+S"));
connect(showStatsAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetBottomPanelVisible(true);
        m_panelManager->SetActiveBottomTab(1);
    }
});
```

### Change 5: Update SaveLayout/RestoreLayout
```cpp
void EditorMainWindow::SaveLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    settings.setValue("Geometry", saveGeometry());
    if (m_panelManager) {
        m_panelManager->SavePanelState(settings);
    }
    settings.sync();
}

void EditorMainWindow::RestoreLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    if (!restoreGeometry(settings.value("Geometry").toByteArray())) {
        resize(1440, 900);
    }
    if (m_panelManager) {
        m_panelManager->RestorePanelState(settings);
    }
}
```

## Compilation Command

```bash
cd build-mingw
cmake --build . --clean-first -- -j 8
```

Or run the CMake build task in VS Code:
- Ctrl+Shift+B → Select "CMake: Build (MinGW)"

## Testing Checklist

After compilation:

- [ ] Editor window opens without errors
- [ ] All three panel groups visible (Left, Right, Bottom)
- [ ] Left panel has Hierarchy and Assets tabs
- [ ] Right panel has Inspector, Mesh Preview, Camera Preview, AI Copilot tabs
- [ ] Bottom panel has Console, Stats, Animation, Logic Copilot tabs
- [ ] Clicking tabs switches between panels
- [ ] Dragging splitters resizes panels smoothly
- [ ] Ctrl+` toggles bottom panel visibility
- [ ] Keyboard shortcuts work (Ctrl+H, Ctrl+I, etc.)
- [ ] Layout persists after closing and reopening editor
- [ ] 3D viewport renders correctly in the center
- [ ] All existing functionality works as before

## Performance Verification

- Check FPS counter in viewport - should be smooth (60+ FPS)
- Monitor memory usage - should not increase significantly
- Tab switching should be instant (< 50ms)
- Splitter resizing should be smooth (no lag)

## Troubleshooting Build Issues

### "TabPanelManager not found"
- Verify file paths in CMakeLists.txt
- Ensure include directory is correct

### Compilation errors
- Check all includes at top of EditorMainWindow.cpp
- Verify method names match (CreateTabPanels, not CreateDockPanels)
- Check that all connections are properly formed

### Runtime crashes
- Check that m_panelManager is not null before use
- Verify all panel widgets are created before being added
- Check console output for error messages

## Rollback Plan

If you need to revert:

```bash
git checkout HEAD -- Engine/Editor/include/Aetherion/Editor/EditorMainWindow.h
git checkout HEAD -- Engine/Editor/src/EditorMainWindow.cpp
git clean -fd Engine/Editor/include/Aetherion/Editor/TabPanelManager*
git clean -fd Engine/Editor/src/TabPanelManager.cpp
```

## Next Steps After Implementation

1. **User Testing** - Get feedback from the team
2. **Refinements** - Adjust panel sizes, colors, shortcuts based on feedback
3. **Documentation** - Update user docs and create tutorial video
4. **Future Features** - Add workspace presets, tab customization, etc.

## Support Resources

- **TABBED_UI_REFACTORING.md** - Detailed refactoring guide
- **INTEGRATION_CODE_SNIPPETS.md** - All code changes with context
- **UI_DESIGN_SPECIFICATION.md** - Design details and specifications
- **UI_VISUAL_REFERENCE.md** - Before/after visual comparisons
- **TabPanelManager.h** - API documentation in comments

## Getting Help

If stuck:
1. Check the relevant documentation file above
2. Review the code in TabPanelManager.h/cpp
3. Look at INTEGRATION_CODE_SNIPPETS.md for exact code placement
4. Check compilation errors for missing includes or syntax issues

---

**You're ready to implement! Start with Phase 1 and take it step by step.**
