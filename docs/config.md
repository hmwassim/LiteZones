# LiteZones — Config & Data Formats

All files live under `%LOCALAPPDATA%\LiteZones\` (FancyZones uses `%LOCALAPPDATA%\Microsoft\PowerToys\FancyZones`; LiteZones uses its own folder).

## Files

| File | Purpose | Written by |
|---|---|---|
| `settings.json` | Global behavior options | LiteZones / user |
| `custom-layouts.json` | User-defined grid/canvas layouts | user / editor |
| `applied-layouts.json` | Which layout applies to each monitor | LiteZones / editor |
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
        "cell-child-map": [[0, 0, 1], [1, 2, 2]],
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

Notes: `X`/`Y` are uppercase (matches FancyZones). Percentages can be integers or floats (FancyZones uses 1/10000 multipliers internally; accept either). `cell-child-map` has `rows` arrays of `columns` child-zone indices; `zone-count` is the max child index + 1. The built-in editor (M5) writes this file via the working-copy mechanism: edits are held in memory and persisted on Apply / Apply-to-all / editor close.

## applied-layouts.json

Assigns a layout to each monitor work area. Template layouts are referenced by type; custom layouts by UUID.

```json
{
  "applied-layouts": [
    {
      "device": {
        "monitor": "\\\\?\\DISPLAY#AUS1030#5&2f1a1a0&0&UID25677#{e6f07b5f-ee97-4a90-b055-3bdf9f0d4d01}",
        "monitor-instance": "",
        "monitor-number": 0,
        "serial-number": "",
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

LiteZones v1 keeps `virtual-desktop` fixed to the empty GUID (no per-desktop layouts). The stable per-monitor lookup key is the `EnumDisplayDevices` **interface string** (`EDD_GET_DEVICE_INTERFACE_NAME`), which already encodes the monitor instance, so the serialized `device.monitor` holds that string and `monitor-instance` is empty; `monitor-number`/`serial-number` are placeholders. Template layouts are referenced by type (e.g. `priority-grid`); custom layouts by their `custom-layouts.json` UUID.

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

`settings.json` and `custom-layouts.json` are hot-reloaded. LiteZones uses `ReadDirectoryChangesW` on the `%LOCALAPPDATA%\LiteZones` directory (single watcher thread, one-shot restartable), posting a message to the main thread. This replaces the `FileWatcher`/`EventWaiter` machinery in `src/common/SettingsAPI`. The editor also triggers an in-process reload via `App::ReloadConfig` after Apply / Apply-to-all / close so work areas rebuild immediately.

## Tray menu

- **Zone snapping** — toggles zone snapping globally.
- **Cycle layout on monitor** — rotates the active monitor through the applied layout's templates/custom layouts (quick experimentation).
- **Edit layouts...** — opens the in-process layout editor (M5): library list (templates + custom), monitor combo, New / Duplicate / Delete / Rename, zone preview (grid separator-drag + double-click split + select/merge, canvas draw/move/resize/delete), and per-monitor **Apply** / **Apply to all**.
- **Reload config** — re-reads all JSON files.
- **Open config folder** — opens `%LOCALAPPDATA%\LiteZones`.
- **Start with Windows** — adds/removes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value `LiteZones`.
- **Exit** — quits.

## Autostart

`HKCU\Software\Microsoft\Windows\CurrentVersion\Run` → `LiteZones` = `"C:\path\to\LiteZones.exe"` (no args; `--no-autostart` skips tray). Uninstall = delete the exe folder + remove the Run key.
