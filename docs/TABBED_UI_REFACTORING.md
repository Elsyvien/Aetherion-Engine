# Editor UI Refactoring: Tabbed Panel Approach

## Overview

The Aetherion Editor has been refactored from a floating dock widget layout to a modern tabbed panel approach. This provides:

- **Cleaner UI**: No overlapping windows or scattered panels
- **Better Organization**: Three clearly defined regions (Left, Right, Bottom)
- **Improved Workflow**: Quick tab switching without window management
- **Professional Look**: Modern tabbed interface like industry-standard editors

## New Layout Structure

```
┌─────────────────────────────────────────────────────────┐
│ Menu Bar (File, Edit, View, Tools, Help)                │
├─────────────────────────────────────────────────────────┤
│ Toolbar (Mode, Gizmo, Play, etc.)                       │
├──────────────────┬────────────────────┬────────────────┤
│                  │                    │                │
│  LEFT PANEL      │   VIEWPORT         │  RIGHT PANEL   │
│  (Tabs:)         │   (3D Rendering)   │  (Tabs:)       │
│  ┌──────────────┐│                    │┌──────────────┐│
│  │ Hierarchy   ││                    ││ Inspector   ││
│  ├──────────────┤│                    │├──────────────┤│
│  │ Asset Brwsr ││                    ││ Mesh Preview││
│  │             ││                    ││ Cam Preview ││
│  │             ││                    ││ AI Copilot  ││
│  │             ││                    ││             ││
│  └──────────────┘│                    │└──────────────┘│
├──────────────────┴────────────────────┴────────────────┤
│ BOTTOM PANEL (Tabs:)                                    │
│ ┌────────────────────────────────────────────────────┐  │
│ │ Console │ Stats │ Animation │ Logic Copilot       │  │
│ ├────────────────────────────────────────────────────┤  │
│ │ Detailed output and information                    │  │
│ └────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────┤
│ Status Bar (FPS, Messages)                               │
└──────────────────────────────────────────────────────────┘
```

## Panel Organization

### Left Panel
- **Hierarchy**: Scene tree navigation
- **Asset Browser**: Asset management and search

### Right Panel
- **Inspector**: Entity component editing
- **Mesh Preview**: Real-time mesh visualization
- **Camera Preview**: Scene camera view
- **AI Copilot**: AI-assisted editing

### Bottom Panel (Collapsible)
- **Console**: Log messages and debug output
- **Statistics**: Performance metrics
- **Animation**: Animation timeline
- **Logic Copilot**: Script logic assistance

## Key Features

### 1. Non-Overlapping Design
All panels are organized in fixed regions with clear splitter boundaries. No floating windows or overlaps.

### 2. Easy Tab Switching
Click tabs to switch between different panels without any window management.

### 3. Collapsible Bottom Panel
The bottom panel can be minimized to maximize viewport space for creative work.

### 4. Responsive Splitters
- Left/Right panels can be resized with horizontal splitter
- Bottom panel height is adjustable with vertical splitter
- All panel sizes are saved/restored on application restart

### 5. Clean Styling
- Dark theme matching VS Code and modern editors
- Highlighted active tabs with blue accent
- Smooth hover effects
- Clear visual hierarchy

## Integration Steps

### Step 1: Update EditorMainWindow.h
Add the TabPanelManager member:
```cpp
class TabPanelManager* m_panelManager = nullptr;
```

Remove all individual QDockWidget pointers and their associated action variables.

### Step 2: Update EditorMainWindow Constructor
Replace the entire dock widget creation with:
```cpp
// Create tabbed panel system
m_panelManager = new TabPanelManager(this);
setCentralWidget(m_panelManager);
m_panelManager->CreatePanelGroups(this);

// Add existing widgets to new tabbed structure
m_panelManager->AddToLeftPanel(m_hierarchyPanel, tr("Hierarchy"));
m_panelManager->AddToLeftPanel(m_assetBrowser, tr("Asset Browser"));

m_panelManager->AddToRightPanel(m_inspectorPanel, tr("Inspector"));
m_panelManager->AddToRightPanel(m_meshPreview, tr("Mesh Preview"));
m_panelManager->AddToRightPanel(m_cameraPreview, tr("Camera Preview"));
m_panelManager->AddToRightPanel(m_copilotPanel, tr("AI Copilot"));

m_panelManager->AddToBottomPanel(m_console, tr("Console"));
m_panelManager->AddToBottomPanel(m_statsPanel, tr("Statistics"));
m_panelManager->AddToBottomPanel(m_animationPanel, tr("Animation"));
m_panelManager->AddToBottomPanel(m_logicCopilotPanel, tr("Logic Copilot"));
```

### Step 3: Update Menu Actions
All "Show/Hide Panel" menu items are replaced with tab switching:
```cpp
// View Menu
auto* viewMenu = menuBar()->addMenu(tr("&View"));

// Left Panel tabs
auto* showHierarchyAction = viewMenu->addAction(tr("Hierarchy\tCtrl+H"));
connect(showHierarchyAction, &QAction::triggered, this, [this]() {
    m_panelManager->SetActiveLeftTab(0);
});

auto* showAssetAction = viewMenu->addAction(tr("Asset Browser\tCtrl+A"));
connect(showAssetAction, &QAction::triggered, this, [this]() {
    m_panelManager->SetActiveLeftTab(1);
});

// Right Panel tabs
auto* showInspectorAction = viewMenu->addAction(tr("Inspector\tCtrl+I"));
connect(showInspectorAction, &QAction::triggered, this, [this]() {
    m_panelManager->SetActiveRightTab(0);
});

// Bottom Panel visibility
auto* toggleBottomAction = viewMenu->addAction(tr("Toggle Bottom Panel\tCtrl+`"));
connect(toggleBottomAction, &QAction::triggered, this, [this]() {
    m_panelManager->SetBottomPanelVisible(!m_panelManager->IsBottomPanelVisible());
});
```

### Step 4: Update Save/Restore Layout
```cpp
void EditorMainWindow::SaveLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    m_panelManager->SavePanelState(settings);
}

void EditorMainWindow::RestoreLayout()
{
    QSettings settings("Aetherion", "AetherionEditor");
    m_panelManager->RestorePanelState(settings);
}
```

### Step 5: Clean Up Unused Code
Remove all references to:
- `QDockWidget` creation and management
- `addDockWidget()` calls
- Individual dock visibility actions
- `tabifyDockWidget()` calls

## Benefits of This Approach

1. **No Panel Overlap**: Every panel has a designated area
2. **Cleaner Code**: Fewer variables and simpler panel management
3. **Better UX**: Consistent with modern IDEs (VS Code, Unreal, Unity)
4. **Easy to Maintain**: All panel layout logic centralized in TabPanelManager
5. **Future Proof**: Can easily add new panels or rearrange tabs
6. **Professional Look**: Modern styling and clear visual hierarchy

## Keyboard Shortcuts

- `Ctrl+H` - Switch to Hierarchy tab
- `Ctrl+A` - Switch to Asset Browser tab
- `Ctrl+I` - Switch to Inspector tab
- `Ctrl+`` - Toggle Bottom Panel visibility
- Tab key when focused on tab bar - Next tab
- Shift+Tab - Previous tab

## Testing Checklist

- [ ] All panels display correctly in their regions
- [ ] Tab switching works smoothly
- [ ] Splitter resizing works for left/right and bottom panels
- [ ] Panel sizes are saved and restored on restart
- [ ] Bottom panel can be collapsed/expanded
- [ ] Keyboard shortcuts work correctly
- [ ] Menu actions switch to correct tabs
- [ ] No overlapping widgets visible
- [ ] Styling is consistent across all tabs
- [ ] Viewport takes up maximum space when bottom panel is hidden

## Future Enhancements

1. **Custom Tab Ordering**: Allow users to drag tabs to reorder them
2. **Tab Grouping**: Allow moving tabs between panels
3. **Saved Workspaces**: Save and load different panel configurations
4. **Floating Tabs**: Option to detach tabs as floating windows
5. **Tab Search**: Quick search to jump to specific tabs
6. **Customizable Shortcuts**: User-defined tab switching shortcuts
