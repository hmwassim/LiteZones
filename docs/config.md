# LiteZones — Config & Data Formats

All files live under `%LOCALAPPDATA%\LiteZones\` (FancyZones uses `%LOCALAPPDATA%\Microsoft\PowerToys\FancyZones`; LiteZones uses its own folder).

## Files

| File | Purpose | Written by |
|---|---|---|
| `settings.json` | Global behavior options | LiteZones / user |
| `custom-layouts.json` | User-defined grid/canvas layouts | user |
| `applied-layouts.json` | Which layout applies to each monitor | LiteZones / user |
| `app-zone-history.json` | Last zone per app (for re-snap on reopen) | LiteZones |

FancyZones also has `layout-hotkeys.json`, `layout-templates.json`, `default-layouts.json`, `zones-settings.json` (legacy). LiteZones folds template + default-layout selection into `settings.json` / `applied-layouts.json` to reduce file count.

## Type tags

Layout `type` values: `blank`, `focus`, `rows`, `columns`, `grid`, `priority-grid`, `custom`.

Template model IDs (from FancyZones): `c_focusModelId=0xFFFF`, `c_rowsModelId=0xFFFE`, `c_columnsModelId=0xFFFD`, `c_gridModelId=0xFFFC`, `c_priorityGridModelId=0xFFFB`, `c_blankCustomModelId=0xFFFA`.

## settings.json

Kebab/snake-case IDs mirror FancyZones (`Settings.cpp` JSON keys):

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
  "zoneHighlightColor": "#AACDFF",
  "zoneNumberColor": "#000000",
  "highlightOpacity": 50,
  "overlappingZonesAlgorithm": "closestCenter",
  "excludedApps": []
}
```

`overlappingZonesAlgorithm`: `smallest`, `largest`, `positional`, `closestCenter`, `enumElements`.

`snapToAppZoneOnOpen` (default `false`): when a process with history opens a new window, it is snapped into its last-used zone. `overrideSnapHotkeys` (default `true`): also respond to `Win+Arrow` (without Ctrl+Alt); when `false`, `Win+Arrow` passes through to the OS window-snap behavior.

### Keyboard snap hotkeys (v1, fixed)

- `Win+Ctrl+Alt+[0-9]` — move focused window into the zone with that number (0 = zone 0, 9 = zone 9).
- `Win+Ctrl+Alt+Left/Right/Up/Down` — move the window one zone in that direction; at an edge it cycles within the monitor, or jumps to the adjacent monitor's first/last zone when `moveWindowAcrossMonitors` is on.
- `Win+Left/Right/Up/Down` — same, only when `overrideSnapHotkeys` is `true`.
- `Up`/`Down` zone movement (by direction/position) requires `moveWindowsBasedOnPosition`; with it off, arrows move by zone index (`Left`/`Right` only).

## custom-layouts.json

```json
{
  "custom-layouts": [
    {
      "uuid": "00000000-0000-0000-0000-000000000001",
      "name": "My Grid",
      "type": "grid",
      "info": {
        "rows": 2,
        "columns": 3,
        "rows-percentage": [50, 50],
        "columns-percentage": [33.33, 33.33, 33.34],
        "cell-child-map": [[0, 0], [0, 0], [0, 1], [1, 1], [1, 2], [1, 2]],
        "show-spacing": true,
        "spacing": 16,
        "sensitivity-radius": 20
      }
    },
    {
      "uuid": "00000000-0000-0000-0000-000000000002",
      "name": "My Canvas",
      "type": "canvas",
      "info": {
        "ref-width": 1920,
        "ref-height": 1080,
        "zones": [
          { "X": 0, "Y": 0, "width": 960, "height": 1080 },
          { "X": 960, "Y": 0, "width": 960, "height": 540 }
        ],
        "sensitivity-radius": 20
      }
    }
  ]
}
```

Notes: `X`/`Y` are uppercase (matches FancyZones). Percentages can be integers or floats (FancyZones uses 1/10000 multipliers internally; accept either).

## applied-layouts.json

Assigns a layout to each monitor work area. Template layouts are referenced by type; custom layouts by UUID.

```json
{
  "applied-layouts": [
    {
      "device": {
        "monitor": "\\\\.\\DISPLAY1",
        "monitor-instance": "1",
        "monitor-number": 1,
        "serial-number": "ABCD1234",
        "virtual-desktop": "00000000-0000-0000-0000-000000000000"
      },
      "applied-layout": {
        "uuid": "00000000-0000-0000-0000-00000000FFFB",
        "type": "priority-grid",
        "show-spacing": true,
        "spacing": 16,
        "zone-count": 3,
        "sensitivity-radius": 20
      }
    }
  ]
}
```

LiteZones v1 keeps `virtual-desktop` fixed to the empty GUID (no per-desktop layouts) and derives a stable monitor key from `EnumDisplayMonitors` device name + instance.

## app-zone-history.json

v1 keys history by app process path only (layouts are uniform across monitors, so a single zone index set per app suffices; no per-monitor/per-virtual-desktop history).

```json
{
  "app-zone-history": [
    {
      "app-path": "C:\\Windows\\System32\\notepad.exe",
      "zone-index-set": [0, 1]
    }
  ]
}
```

`zone-index-set` is an array of zone indices the window was last snapped to (single-int form is not emitted by v1). Written on every snap; consumed when a new window for the app appears and `snapToAppZoneOnOpen` is enabled.

## File watching

`settings.json` and `custom-layouts.json` are hot-reloaded. LiteZones uses `ReadDirectoryChangesW` on the `%LOCALAPPDATA%\LiteZones` directory (single watcher thread, one-shot restartable), posting a message to the main thread. This replaces the `FileWatcher`/`EventWaiter` machinery in `src/common/SettingsAPI`.

## Tray menu (editor substitute)

Since there is no graphical editor in v1:

- **Enable / Disable** — toggles zone snapping globally.
- **Cycle layout on monitor** — rotates the active monitor through the applied layout's templates/custom layouts (quick experimentation).
- **Reload config** — re-reads all JSON files.
- **Open config folder** — opens `%LOCALAPPDATA%\LiteZones`.
- **Autostart** — adds/removes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value `LiteZones`.
- **Exit** — quits.

## Autostart

`HKCU\Software\Microsoft\Windows\CurrentVersion\Run` → `LiteZones` = `"C:\path\to\LiteZones.exe"` (no args; `--no-autostart` skips tray). Uninstall = delete the exe folder + remove the Run key.
