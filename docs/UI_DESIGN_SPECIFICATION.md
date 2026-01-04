# UI/UX Redesign Specification

## Design Philosophy

The new tabbed interface follows these principles:
1. **Clarity**: Every UI element has a clear purpose and location
2. **Efficiency**: No wasted space, no overlapping dialogs
3. **Modernity**: Matches current industry standards (VS Code, Unreal, Unity)
4. **Flexibility**: Easy to hide/show and resize panels
5. **Consistency**: Unified styling and interaction patterns

## Color Scheme

The tabbed interface uses a dark professional theme:

```
Background:      #1e1e1e (Dark Gray)
Panel BG:        #1e1e1e (Dark Gray)
Tab Inactive:    #2d2d2d (Medium Gray)
Tab Active:      #0d47a1 (Deep Blue)
Tab Accent:      #1976d2 (Light Blue)
Text Primary:    #cccccc (Light Gray)
Text Secondary:  #aaaaaa (Medium Gray)
Splitter:        #3a3a3a (Dark Gray)
Border:          #1a1a1a (Very Dark Gray)
```

## Layout Specifications

### Viewport (Center)
- Takes maximum available space
- Resizable via splitters on all sides
- Grid overlay option (enabled/disabled via View menu)
- FPS counter in corner
- Selection info overlay

### Left Panel Group
- **Width**: 200px minimum, 600px maximum (configurable)
- **Tabs**:
  1. Hierarchy Panel
  2. Asset Browser
- **Features**:
  - Quick search in both tabs
  - Drag-drop support for asset placement
  - Right-click context menus
  - Expand/collapse tree nodes

### Right Panel Group
- **Width**: 250px minimum, 800px maximum (configurable)
- **Tabs**:
  1. Inspector (default active)
  2. Mesh Preview
  3. Camera Preview  
  4. AI Copilot
- **Features**:
  - Property editing with live preview
  - Component addition/removal
  - Real-time validation
  - Scroll-to-fit for long property lists

### Bottom Panel (Collapsible)
- **Height**: 150px minimum, 500px maximum (configurable)
- **Default**: Hidden or minimized
- **Tabs**:
  1. Console (Default active)
  2. Statistics
  3. Animation Panel
  4. Logic Copilot
- **Features**:
  - Collapsible to maximize viewport
  - Keyboard toggle (Ctrl+`)
  - Message filtering in Console
  - Performance graph in Statistics
  - Timeline in Animation Panel

## Spacing & Padding

All UI elements follow consistent spacing:

```
Panel padding:       8px
Tab padding:         8px horizontal, 6px vertical
Splitter width:      4px
Component spacing:   4px
Dialog margins:      12px
Form row spacing:    4px
```

## Typography

- **Font Family**: System default (Segoe UI on Windows)
- **Tab Text**: 10pt, normal weight
- **Panel Title**: 11pt, bold
- **Body Text**: 10pt, normal weight
- **Monospace**: 9pt (Console messages)

## Interaction Patterns

### Tab Switching
- Click on tab header to activate
- Keyboard: Ctrl+Tab (next), Ctrl+Shift+Tab (previous)
- Custom shortcuts for specific tabs (see Keyboard Shortcuts section)

### Resizing
- Drag splitter handles to resize panels
- Double-click splitter to auto-fit content
- Right-click splitter for context menu options

### Hiding/Showing
- Click X button on tab (if enabled) to hide panel
- View menu > Toggle [Panel Name]
- Keyboard shortcuts for quick toggling

### Collapsing
- Bottom panel can be collapsed to maximize viewport
- Left/Right panels can be fully hidden
- All panel states saved to configuration

## Responsive Behavior

The UI adapts to different window sizes:

- **Small (< 1200px)**: Hide left panel, narrow right panel
- **Medium (1200-1920px)**: All panels visible, default sizing
- **Large (> 1920px)**: Expand right panel, wide inspector
- **Ultrawide**: Show all panels with maximum comfortable width

## Animation & Transitions

Smooth transitions enhance the user experience:

- **Tab Switch**: 200ms fade transition
- **Panel Resize**: Smooth drag with real-time preview
- **Hover Effects**: 150ms background color transition
- **Active Indicator**: Instant tab highlight

## Accessibility

The interface provides:

- **Keyboard Navigation**: Full keyboard access to all panels
- **High Contrast**: Dark theme reduces eye strain
- **Clear Focus**: Visual focus indicator on all interactive elements
- **Logical Tab Order**: Sensible keyboard focus flow
- **Tooltips**: Helpful hints for complex controls

## Platform-Specific Considerations

### Windows
- Native Windows 10/11 style splitters
- Standard system font
- DirectInput for gizmo interaction

### macOS (Future Support)
- Native macOS style controls
- San Francisco system font
- Trackpad gesture support

### Linux (Future Support)
- Platform-agnostic styling
- Font fallback chain
- Standard X11 interactions

## Mobile/Touch Considerations (Future)

While the initial design targets desktop, it's structured for potential touch:

- Large touch targets (minimum 44px)
- Swipe gestures for tab switching
- Long-press for context menus
- Pinch to resize panels

## Customization Options

Users can customize:

1. **Panel Visibility**: Show/hide any panel
2. **Tab Ordering**: Rearrange tabs within panels
3. **Color Theme**: Dark/Light theme toggle
4. **Font Size**: Adjust UI scale (80%-150%)
5. **Keyboard Shortcuts**: Rebind shortcuts
6. **Workspace Presets**: Save/load custom layouts

## Migration Path

Existing functionality is preserved:

- All dock widgets are converted to tabs
- All actions work identically
- Keyboard shortcuts remain the same
- Configuration files are migrated automatically
- No data loss or workflow disruption

## Performance Impact

The tabbed interface is more efficient:

- Fewer QWidget instances (fewer allocations)
- Simpler event handling (no floating window management)
- Reduced paint operations (no overlapping windows)
- Better memory usage (cached tab widgets)
- Smoother animations (GPU-accelerated when available)

## Code Structure

```cpp
EditorMainWindow
├── MenuBar
├── ToolBar
├── TabPanelManager
│   ├── LeftPanel (QTabWidget)
│   │   ├── EditorHierarchyPanel
│   │   └── EditorAssetBrowser
│   ├── CentralSplitter (QSplitter: Horizontal)
│   │   ├── LeftPanel
│   │   ├── EditorViewport (central)
│   │   └── RightPanel
│   ├── RightPanel (QTabWidget)
│   │   ├── EditorInspectorPanel
│   │   ├── EditorMeshPreview
│   │   ├── EditorCameraPreview
│   │   └── AICopilotPanel
│   └── VerticalSplitter (QSplitter: Vertical)
│       ├── HorizontalSplitter
│       └── BottomPanel (QTabWidget)
│           ├── EditorConsole
│           ├── EditorStatisticsPanel
│           ├── EditorAnimationPanel
│           └── EditorLogicCopilotPanel
└── StatusBar
```

## Configuration Files

Layout configuration is saved in:

Windows: `%APPDATA%/Aetherion/AetherionEditor/config.ini`
Linux: `~/.config/Aetherion/AetherionEditor/config.ini`
macOS: `~/Library/Preferences/com.Aetherion.AetherionEditor.plist`

Saved settings include:

```ini
[PanelLayout]
LeftPanelCurrentIndex=0
RightPanelCurrentIndex=0
BottomPanelCurrentIndex=0
BottomPanelVisible=true

[PanelSizes]
LeftPanelWidth=250
RightPanelWidth=300
BottomPanelHeight=200

[Geometry]
WindowWidth=1920
WindowHeight=1080
WindowX=0
WindowY=0
IsMaximized=true
```

## Testing Strategy

Comprehensive testing covers:

1. **Functional Tests**: All panel switching works
2. **Layout Tests**: Panels resize and align correctly
3. **Integration Tests**: Panels communicate with viewport
4. **Performance Tests**: No lag during resizing or switching
5. **Compatibility Tests**: Works on Windows 10/11, macOS, Linux
6. **Regression Tests**: All existing features still work

## Success Criteria

The redesign is successful when:

✓ No overlapping UI elements
✓ All panels are clearly organized
✓ Tab switching is smooth and responsive
✓ Panels can be resized without issues
✓ Layout is saved and restored correctly
✓ Professional appearance matches industry standards
✓ Users report improved workflow efficiency
✓ No performance degradation
✓ All existing features work identically
✓ Codebase is cleaner and more maintainable
