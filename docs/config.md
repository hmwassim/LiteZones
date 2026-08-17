# LiteZones — Config & Data Formats

All files live under `%LOCALAPPDATA%\LiteZones\`.

## Files

| File | Purpose | Written by |
|---|---|---|
| `settings.json` | Global behavior options | LiteZones / user |
| `custom-layouts.json` | User-defined grid/canvas layouts | user / editor |
| `applied-layouts.json` | Which layout applies to each monitor | LiteZones / editor |
| `app-zone-history.json` | Last zone per app (for re-snap on reopen) | LiteZones |

## settings.json

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
| `excludedApps` | `[]` | Process paths to exclude from snapping |

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

`X`/`Y` are uppercase. Percentages use 1/10000 multipliers internally. `cell-child-map` has `rows` arrays of `columns` child-zone indices; `zone-count` is the max child index + 1. The built-in editor writes this file via the working-copy mechanism: edits are held in memory and persisted on Apply / Apply-to-all / editor close.

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

The stable per-monitor lookup key is the `EnumDisplayDevices` interface string. Template layouts are referenced by type (e.g. `priority-grid`); custom layouts by their `custom-layouts.json` UUID.

## app-zone-history.json

Keys history by app process path only.

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

`zone-index-set` is an array of zone indices the window was last snapped to. Written on every snap.

## File watching

`settings.json` and `custom-layouts.json` are hot-reloaded via `ReadDirectoryChangesW` on the config directory. The editor also triggers an in-process reload via `App::ReloadConfig` after Apply / Apply-to-all / close.

## Tray menu

- **Zone snapping** — toggles zone snapping globally.
- **Cycle layout on monitor** — rotates the active monitor through applied layouts.
- **Edit layouts...** — opens the in-process layout editor (library list, monitor combo, New/Duplicate/Delete/Rename, zone preview, per-monitor Apply / Apply to all).
- **Reload config** — re-reads all JSON files.
- **Open config folder** — opens `%LOCALAPPDATA%\LiteZones`.
- **Start with Windows** — adds/removes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value.
- **Exit** — quits.

## Autostart

`HKCU\Software\Microsoft\Windows\CurrentVersion\Run` → `LiteZones` = `"C:\path\to\LiteZones.exe"`. Uninstall = delete the exe folder + remove the Run key.
