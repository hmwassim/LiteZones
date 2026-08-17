# LiteZones

A lightweight zone-snapping tool for Windows. Single native C++17 executable with zero runtime dependencies.

## Features

- **Shift + drag** to snap windows into zone layouts
- **Per-monitor layouts** — each display runs its own independent zone grid
- **Layout editor** — visual editor for custom grid and freeform canvas layouts
- **Hot reload** — edit `settings.json` and changes apply instantly
- **System tray** — toggle snapping, cycle layouts, edit configs
- **Autostart** — optional Start with Windows via registry
- **Span across monitors** — combine all displays into one unified workspace
- **App zone history** — remembers where each app was last snapped

## Quick Start

### Build

Requires [Visual Studio 2022](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload.

```cmd
tools\build.cmd
```

Output: `bin\x64\Release\LiteZones.exe`

### Run

Double-click the exe. A tray icon appears in the notification area.

### Snap a Window

Hold **Shift** and drag a window. The zone overlay appears — drop to snap.

| Shortcut | Action |
|---|---|
| **Shift + drag** | Snap to zone |
| **Ctrl + drag** | Span multiple zones |
| **Middle-click + drag** | Span multiple zones (alternative) |

### Edit Layouts

Right-click the tray icon and select **Edit layouts...**

**Grid mode:**
- Drag resizers to resize zones
- Double-click to split a zone
- Ctrl+click to multi-select, then right-click to merge

**Canvas mode:**
- Drag empty space to draw a zone
- Drag a zone to move it
- Drag handles to resize
- Delete key or right-click to remove

### Uninstall

1. Exit LiteZones from the tray menu
2. Delete the exe folder
3. Remove the `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\LiteZones` registry key (if autostart was enabled)
4. Optionally delete `%LOCALAPPDATA%\LiteZones\`

## Configuration

All config lives in `%LOCALAPPDATA%\LiteZones\`.

### settings.json

```json
{
  "shiftDrag": true,
  "mouseSwitch": false,
  "mouseMiddleClickSpanningMultipleZones": false,
  "moveWindowAcrossMonitors": false,
  "restoreSize": true,
  "spanZonesAcrossMonitors": false,
  "makeDraggedWindowTransparent": false,
  "showZoneNumber": true,
  "highlightOpacity": 50,
  "zoneColor": "#AACDFF",
  "zoneBorderColor": "#FFFFFF",
  "zoneHighlightColor": "#FFFFFF",
  "zoneNumberColor": "#000000",
  "overlappingZonesAlgorithm": "closestCenter",
  "excludedApps": []
}
```

| Key | Default | Description |
|---|---|---|
| `shiftDrag` | `true` | Hold Shift while dragging to activate zone snapping |
| `mouseSwitch` | `false` | Right-click activates zone snapping (XOR with Shift) |
| `mouseMiddleClickSpanningMultipleZones` | `false` | Middle-click enables multi-zone spanning |
| `moveWindowAcrossMonitors` | `false` | Allow dragging windows across monitors |
| `restoreSize` | `true` | Restore window size when unsnapping |
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
bin\x64\Release\ZoneTests.exe
```

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
  TrayService.cpp       System tray icon and menu
  ZoneAssignmentStore.cpp  Unified zone assignment persistence
  json.cpp              Hand-rolled JSON parser/serializer
  GuidUtils.cpp         GUID parsing/formatting and layout type helpers
  ...                   Zone math, window utils, colors, paths, etc.

tests/ZoneTests/        Unit test suites (no framework dependencies)
tools/build.cmd         MSBuild wrapper (auto-locates VS 2022)
```

## Stats

| Metric | Value |
|---|---|
| Release exe | ~449 KB |
| Idle RAM | ~10 MB working set |
| Idle CPU | 0% (event-driven) |
| Dependencies | Zero (static `/MT`, hand-rolled JSON) |
| Overlay | Direct2D, rendered only while dragging |
