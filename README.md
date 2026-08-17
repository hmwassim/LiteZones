# LiteZones

Standalone Windows tiling window manager. Single native C++17/Win32 executable -- no PowerToys runner, no telemetry, no installer.

| Metric | Value |
|---|---|
| Release exe | ~447 KB |
| Idle RAM | ~10 MB working set |
| Idle CPU | 0% (event-driven) |
| Dependencies | Zero (static `/MT`, hand-rolled JSON) |
| Overlay | Direct2D, rendered only while dragging |
| Tests | 29 unit test suites |

## Quick Start

**Build** (requires Visual Studio 2022 with "Desktop development with C++"):

```cmd
tools\build.cmd Release
```

Output: `bin\x64\Release\LiteZones.exe`

**Run**: double-click the exe. A tray icon appears.

**Snap a window**: hold **Shift** and drag -- zone overlay appears, drop to snap.

**Edit layouts**: right-click tray icon > **Edit layouts...**

**Autostart**: tray menu > **Start with Windows** (writes to `HKCU\...\Run`).

**Uninstall**: delete the exe folder, remove the `HKCU\...\Run` key, optionally delete `%LOCALAPPDATA%\LiteZones\`.

## Features

### Drag-to-Snap

- **Shift + drag** a window to see the zone overlay and snap it
- **Ctrl + drag** or **Middle-click + drag** to span multiple zones
- **Right-click** toggles secondary mode (when `mouseSwitch` is enabled)
- Unsnap restores the original window size and position
- Optional window transparency while dragging (`makeDraggedWindowTransparent`)

### Keyboard Snap

- **Win+Ctrl+Alt+[0-9]** -- snap focused window to zone number
- **Win+Ctrl+Alt+Left/Right** -- move window to adjacent zone (by index)
- **Win+Ctrl+Alt+Up/Down** -- move window by spatial position
- **Win+Arrow** -- same as above when `overrideSnapHotkeys` is true (replaces OS snap)
- Cross-monitor cycling supported

### Layout Editor

Non-modal editor with a library of templates + custom layouts.

**Grid mode** (for grid layouts):
- Drag resizers to resize zones
- Double-click to split a zone 2x2
- Ctrl+click to multi-select, then right-click to merge
- Arrow keys to fine-tune resizer positions

**Canvas mode** (for freeform layouts):
- Drag empty space to draw a new zone
- Drag a zone to move it, drag handles to resize
- Delete key or right-click to remove zones
- Arrow keys to nudge zone edges by 1px

**Menu bar**:

| Action | Shortcut |
|---|---|
| Save | Ctrl+S |
| Apply to Monitor | Ctrl+Enter |
| Apply to All Monitors | Ctrl+Shift+Enter |
| Undo | Ctrl+Z (50 levels) |
| Close | Alt+F4 / Escape |

### Per-Monitor Layouts

Each monitor can run its own independent layout. Monitors are identified by stable `EnumDisplayDevices` interface strings (no WMI, no volatile IDs).

### Span-Across-Monitors

Optional mode that combines all monitors into a single work area for one unified layout.

### App Zone History

Remembers the last zone each app was snapped into. Optionally auto-snap new windows to their last-used zone on open.

### System Tray

- **Zone snapping** -- toggle globally
- **Cycle layout on monitor** -- rotate through all layouts on the monitor under the cursor
- **Edit layouts...** -- open the layout editor
- **Reload config** -- re-read all JSON files
- **Open config folder** -- open `%LOCALAPPDATA%\LiteZones` in Explorer
- **Start with Windows** -- toggle autostart
- **Exit**

### Hot Reload

`settings.json`, `custom-layouts.json`, and `applied-layouts.json` are watched via `ReadDirectoryChangesW`. Edit any file and changes apply instantly.

## Configuration

All config lives in `%LOCALAPPDATA%\LiteZones\`.

### settings.json

```json
{
  "shiftDrag": true,
  "mouseSwitch": false,
  "mouseMiddleClickSpanningMultipleZones": false,
  "moveWindowAcrossMonitors": false,
  "moveWindowsBasedOnPosition": false,
  "snapToAppZoneOnOpen": false,
  "overrideSnapHotkeys": true,
  "restoreSize": true,
  "openWindowOnActiveMonitor": false,
  "spanZonesAcrossMonitors": false,
  "makeDraggedWindowTransparent": false,
  "showZoneNumber": true,
  "zoneColor": "#AACDFF",
  "zoneBorderColor": "#FFFFFF",
  "zoneHighlightColor": "#FFFFFF",
  "zoneNumberColor": "#000000",
  "highlightOpacity": 50,
  "overlappingZonesAlgorithm": "closestCenter",
  "excludedApps": []
}
```

| Key | Default | Description |
|---|---|---|
| `shiftDrag` | `true` | Hold Shift while dragging to activate zone snapping |
| `mouseSwitch` | `false` | Right-click can activate zone snapping (XOR with Shift) |
| `mouseMiddleClickSpanningMultipleZones` | `false` | Middle-click enables multi-zone spanning |
| `moveWindowAcrossMonitors` | `false` | Allow dragging/cycling windows across monitors |
| `moveWindowsBasedOnPosition` | `false` | Arrow keys use spatial position instead of zone index |
| `snapToAppZoneOnOpen` | `false` | Auto-snap new windows to their last-used zone |
| `overrideSnapHotkeys` | `true` | Intercept Win+Arrow (without Ctrl+Alt) |
| `restoreSize` | `true` | Restore window size when unsnapping |
| `openWindowOnActiveMonitor` | `false` | Open new windows on the active monitor |
| `spanZonesAcrossMonitors` | `false` | Combine all monitors into a single work area |
| `makeDraggedWindowTransparent` | `false` | 50% alpha on dragged windows |
| `showZoneNumber` | `true` | Display zone numbers in the overlay |
| `highlightOpacity` | `50` | Zone highlight opacity (0-100) |
| `zoneColor` | `"#AACDFF"` | Zone fill color |
| `zoneBorderColor` | `"#FFFFFF"` | Zone border color |
| `zoneHighlightColor` | `"#FFFFFF"` | Highlighted zone color |
| `zoneNumberColor` | `"#000000"` | Zone number text color |
| `overlappingZonesAlgorithm` | `"closestCenter"` | `closestCenter`, `smallest`, `largest`, or `positional` |
| `excludedApps` | `[]` | Process paths to exclude from snapping |

### Data Files

| File | Contents |
|---|---|
| `settings.json` | Global behavior options |
| `custom-layouts.json` | User-defined grid and canvas layouts |
| `applied-layouts.json` | Per-monitor layout assignments |
| `app-zone-history.json` | Last zone per app (for re-snap on reopen) |

## Building

### Prerequisites

- **Visual Studio 2022** with "Desktop development with C++" workload
- Windows 10 SDK

### Commands

```cmd
tools\build.cmd Release     bin\x64\Release\LiteZones.exe
tools\build.cmd Debug       bin\x64\Debug\LiteZones.exe
```

### Tests

```cmd
bin\x64\Debug\ZoneTests.exe
```

29 test suites covering: zone math, layout engine (all template types), grid data operations, canvas geometry, GUID helpers, custom/applied layout stores, app zone history.

### Build Configuration

- C++17, `/W4 /WX`, Unicode, static runtime (`/MT`)
- Links: `d2d1.lib`, `dwrite.lib`, `dwmapi.lib`, `ole32.lib`, `msimg32.lib`
- Subsystem: Windows (no console window)

## Project Structure

```
src/litezones/
  main.cpp              Entry point, single-instance mutex, DPI awareness
  App.cpp               Tray icon, message dispatch, config reload
  Hooks.cpp             Global keyboard/mouse/WinEvent hooks
  DragController.cpp    Drag-to-snap interaction
  KeyboardSnap.cpp      Keyboard zone snap and arrow navigation
  ZonesOverlay.cpp      Direct2D zone overlay renderer
  WorkArea.cpp          Per-monitor zone container
  WorkAreaManager.cpp   Work area lifecycle and layout resolution
  MonitorManager.cpp    Monitor enumeration, ordering, DPI
  LayoutEngine.cpp      Layout configurators (all template types)
  EditorWindow.cpp      Layout editor window
  EditorCanvas.cpp      Editor canvas (grid-edit and canvas-edit)
  GridData.cpp          Grid editor model
  Settings.cpp          Settings singleton and persistence
  CustomLayouts.cpp     Custom layout store
  AppliedLayouts.cpp    Per-monitor layout assignments
  AppZoneHistory.cpp    App-to-zone mapping persistence
  json.cpp              Hand-rolled JSON parser/serializer
  ...                   Zone math, window utils, colors, paths, etc.

tests/ZoneTests/        29 unit test suites (no framework dependencies)
tools/build.cmd         MSBuild wrapper (auto-locates VS 2022)
docs/                   Architecture analysis, implementation plan, config reference
```

## Docs

- [plan.md](docs/plan.md) -- implementation milestones and performance budget
- [architecture.md](docs/architecture.md) -- FancyZones internals analysis
- [config.md](docs/config.md) -- JSON config formats and storage paths
