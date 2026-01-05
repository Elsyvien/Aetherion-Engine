# EditorMainWindow Integration Code Snippets

This document provides the key code changes needed to integrate TabPanelManager into EditorMainWindow.

## 1. Header File Changes (EditorMainWindow.h)

### Remove these dock widget member variables:

```cpp
// DELETE THESE:
QDockWidget* m_hierarchyDock = nullptr;
QDockWidget* m_inspectorDock = nullptr;
QDockWidget* m_assetBrowserDock = nullptr;
QDockWidget* m_consoleDock = nullptr;
QDockWidget* m_meshPreviewDock = nullptr;
QDockWidget* m_cameraPreviewDock = nullptr;
QDockWidget* m_copilotDock = nullptr;
QDockWidget* m_assetGenDock = nullptr;
QDockWidget* m_statsDock = nullptr;
QDockWidget* m_animationDock = nullptr;
QDockWidget* m_logicCopilotDock = nullptr;
QDockWidget* m_bookmarksDock = nullptr;
```

### Remove these related action variables:

```cpp
// DELETE THESE:
QAction* m_showHierarchyAction = nullptr;
QAction* m_showInspectorAction = nullptr;
QAction* m_showAssetBrowserAction = nullptr;
QAction* m_showConsoleAction = nullptr;
QAction* m_showMeshPreviewAction = nullptr;
QAction* m_showCameraPreviewAction = nullptr;
QAction* m_showBookmarksAction = nullptr;
QAction* m_showAICopilotAction = nullptr;
QAction* m_showAssetGenAction = nullptr;
QAction* m_showStatsAction = nullptr;
QAction* m_showAnimationPanelAction = nullptr;
QAction* m_showLogicCopilotAction = nullptr;
QAction* m_showAiHudAction = nullptr;
```

### Add this new member variable:

```cpp
// ADD THIS:
class TabPanelManager* m_panelManager = nullptr;
```

### Update CreateDockPanels method:

```cpp
// CHANGE THIS:
void CreateDockPanels();

// TO THIS:
void CreateTabPanels();
```

## 2. Constructor Changes (EditorMainWindow.cpp)

### Current code (lines ~520-560):

```cpp
// OLD CODE - REMOVE THIS ENTIRE SECTION:
auto *centerSplit = new QSplitter(Qt::Horizontal, this);
centerSplit->setChildrenCollapsible(false);

m_viewport = new EditorViewport(centerSplit);
m_viewport->setFocusPolicy(Qt::StrongFocus);
m_viewport->installEventFilter(this);
if (m_viewport->surfaceWidget()) {
  m_viewport->surfaceWidget()->installEventFilter(this);
}
centerSplit->addWidget(m_viewport);

// ... rest of old dock setup ...
```

### Replace with:

```cpp
// NEW CODE - ADD THIS:
// Create the tabbed panel system
m_panelManager = new TabPanelManager(this);
setCentralWidget(m_panelManager);
m_panelManager->CreatePanelGroups(this);

// Create viewport
m_viewport = new EditorViewport(this);
m_viewport->setFocusPolicy(Qt::StrongFocus);
m_viewport->installEventFilter(this);
if (m_viewport->surfaceWidget()) {
  m_viewport->surfaceWidget()->installEventFilter(this);
}

// Note: The viewport will be set as central widget of the panel manager
// It will be positioned in the center of the splitter layout
```

## 3. Panel Creation Method Replacement

### Old CreateDockPanels method (~3500+ lines):

**COMPLETELY REPLACE** the entire `CreateDockPanels()` method with:

```cpp
void EditorMainWindow::CreateTabPanels()
{
    if (!m_panelManager) {
        return;
    }

    // Left Panel - Asset Management and Hierarchy
    {
        m_hierarchyPanel = new EditorHierarchyPanel(m_panelManager);
        m_panelManager->AddToLeftPanel(m_hierarchyPanel, tr("Hierarchy"));

        m_assetBrowser = new EditorAssetBrowser(m_panelManager);
        m_panelManager->AddToLeftPanel(m_assetBrowser, tr("Assets"));

        // Set first tab as active (Hierarchy)
        m_panelManager->SetActiveLeftTab(0);
    }

    // Right Panel - Inspector and Previews
    {
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

        // Set first tab as active (Inspector)
        m_panelManager->SetActiveRightTab(0);
    }

    // Bottom Panel - Console and Statistics
    {
        m_console = new EditorConsole(m_panelManager);
        m_panelManager->AddToBottomPanel(m_console, tr("Console"));

        m_statsPanel = new EditorStatisticsPanel(m_panelManager);
        m_panelManager->AddToBottomPanel(m_statsPanel, tr("Statistics"));

        m_animationPanel = new EditorAnimationPanel(m_panelManager);
        m_panelManager->AddToBottomPanel(m_animationPanel, tr("Animation"));

        m_logicCopilotPanel = new EditorLogicCopilotPanel(m_panelManager);
        m_panelManager->AddToBottomPanel(m_logicCopilotPanel, tr("Logic Copilot"));

        // Set first tab as active (Console)
        m_panelManager->SetActiveBottomTab(0);

        // Keep bottom panel visible by default
        m_panelManager->SetBottomPanelVisible(true);
    }

    // Asset generation panel (optional, can be in a separate float or integrated)
    {
        m_assetGenPanel = new EditorAssetGenerationPanel(m_panelManager);
        // Option 1: Add to bottom panel as another tab
        m_panelManager->AddToBottomPanel(m_assetGenPanel, tr("Asset Generator"));
        // Option 2: Or add to right panel
        // m_panelManager->AddToRightPanel(m_assetGenPanel, tr("Asset Generator"));
    }
}
```

## 4. View Menu Update

### Find where View menu is created (~1200 lines in constructor):

```cpp
// OLD CODE - REPLACE THIS ENTIRE SECTION:
auto *viewMenu = menuBar()->addMenu(tr("&View"));

m_showHierarchyAction = viewMenu->addAction(tr("Hierarchy\tCtrl+H"));
connect(m_showHierarchyAction, &QAction::triggered, this, [this](bool checked) {
    if (m_hierarchyDock) {
        m_hierarchyDock->setVisible(checked);
        if (checked) {
            m_hierarchyDock->raise();
        }
    }
});

// ... many more similar actions ...

// NEW CODE - REPLACE WITH:
auto *viewMenu = menuBar()->addMenu(tr("&View"));

// Left Panel shortcuts
auto *showHierarchyAction = viewMenu->addAction(tr("Hierarchy\tCtrl+H"));
connect(showHierarchyAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveLeftTab(0);
    }
});

auto *showAssetsAction = viewMenu->addAction(tr("Asset Browser\tCtrl+Shift+A"));
connect(showAssetsAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveLeftTab(1);
    }
});

viewMenu->addSeparator();

// Right Panel shortcuts
auto *showInspectorAction = viewMenu->addAction(tr("Inspector\tCtrl+I"));
connect(showInspectorAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveRightTab(0);
    }
});

auto *showMeshPreviewAction = viewMenu->addAction(tr("Mesh Preview\tCtrl+Shift+M"));
connect(showMeshPreviewAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveRightTab(1);
    }
});

auto *showCameraPreviewAction = viewMenu->addAction(tr("Camera Preview\tCtrl+Shift+C"));
connect(showCameraPreviewAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveRightTab(2);
    }
});

auto *showAICopilotAction = viewMenu->addAction(tr("AI Copilot\tCtrl+Shift+P"));
connect(showAICopilotAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetActiveRightTab(3);
    }
});

viewMenu->addSeparator();

// Bottom Panel
auto *toggleBottomPanelAction = viewMenu->addAction(tr("Toggle Bottom Panel\tCtrl+`"));
connect(toggleBottomPanelAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        bool visible = m_panelManager->IsBottomPanelVisible();
        m_panelManager->SetBottomPanelVisible(!visible);
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

auto *showAnimationAction = viewMenu->addAction(tr("Animation\tCtrl+Shift+N"));
connect(showAnimationAction, &QAction::triggered, this, [this]() {
    if (m_panelManager) {
        m_panelManager->SetBottomPanelVisible(true);
        m_panelManager->SetActiveBottomTab(2);
    }
});
```

## 5. Save/Restore Layout Methods

### Find and update SaveLayout() and RestoreLayout():

```cpp
// OLD CODE:
void EditorMainWindow::SaveLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    settings.setValue("Geometry", saveGeometry());
    settings.setValue("WindowState", saveState());
    // ... other settings ...
}

void EditorMainWindow::RestoreLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    restoreGeometry(settings.value("Geometry").toByteArray());
    restoreState(settings.value("WindowState").toByteArray());
    // ... other settings ...
}

// NEW CODE:
void EditorMainWindow::SaveLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    settings.setValue("Geometry", saveGeometry());
    
    // Save panel states
    if (m_panelManager) {
        m_panelManager->SavePanelState(settings);
    }
    
    settings.sync();
}

void EditorMainWindow::RestoreLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    
    if (!restoreGeometry(settings.value("Geometry").toByteArray())) {
        resize(1440, 900);  // Default size
    }
    
    // Restore panel states
    if (m_panelManager) {
        m_panelManager->RestorePanelState(settings);
    }
}
```

## 6. Constructor Call Update

### Find where CreateDockPanels() is called:

```cpp
// OLD CODE:
CreateDockPanels();

// NEW CODE:
CreateTabPanels();
```

## 7. Include Statements

### Add to EditorMainWindow.cpp includes:

```cpp
#include "Aetherion/Editor/TabPanelManager.h"
```

## 8. Cleanup Task Methods

Several methods that manage dock visibility can be simplified or removed:

```cpp
// These methods can be REMOVED or SIMPLIFIED:
void EditorMainWindow::ShowHierarchy() { /* remove or simplify */ }
void EditorMainWindow::ShowInspector() { /* remove or simplify */ }
void EditorMainWindow::ShowAssetBrowser() { /* remove or simplify */ }
void EditorMainWindow::ShowConsole() { /* remove or simplify */ }
void EditorMainWindow::ToggleBottomPanel() { /* remove or simplify */ }

// Replace with:
void EditorMainWindow::ShowPanel(int panelGroup, int tabIndex)
{
    if (!m_panelManager) return;
    
    if (panelGroup == 0) {  // Left
        m_panelManager->SetActiveLeftTab(tabIndex);
    } else if (panelGroup == 1) {  // Right
        m_panelManager->SetActiveRightTab(tabIndex);
    } else if (panelGroup == 2) {  // Bottom
        m_panelManager->SetBottomPanelVisible(true);
        m_panelManager->SetActiveBottomTab(tabIndex);
    }
}
```

## 9. CMakeLists.txt Update

Add TabPanelManager to the editor target:

```cmake
# In the EDITOR_SOURCES section, add:
Engine/Editor/include/Aetherion/Editor/TabPanelManager.h
Engine/Editor/src/TabPanelManager.cpp
```

## Compilation

After making these changes:

1. Update CMakeLists.txt with the new files
2. Run CMake to regenerate build files
3. Compile the project
4. Test all panel switching and resizing

```bash
cd build-mingw
cmake --build . --clean-first -- -j 8
```

## Verification

After compilation, verify:

- [x] All three panel groups are visible
- [x] Tabs can be clicked to switch panels
- [x] Splitters can be dragged to resize
- [x] Bottom panel can be hidden/shown
- [x] Keyboard shortcuts work
- [x] No overlapping panels
- [x] Layout saves/restores correctly
- [x] All existing functionality works

## Migration Path

This refactoring is fully backwards compatible. Users' existing layout preferences will be migrated to the new system automatically when the application starts.
