# LiteZones

Lightweight window zone-snapping for Windows. Native C++17, zero dependencies, ~440 KB.

## How to?

1. Run `LiteZones.exe`. A tray icon appears.
2. Hold **Shift** and drag a window. The zone overlay shows — drop to snap.
3. Hold **Shift + Ctrl** (or middle-click + drag) to span multiple zones.
4. Right-click the tray icon to access settings, cycle layouts, or open the layout editor.

## Layout editor

Right-click the tray icon → **Edit layouts...**

- **Grid mode**: drag resizers to resize, double-click to split, Ctrl+click to multi-select then right-click to merge
- **Canvas mode**: drag empty space to draw, drag zones to move, drag handles to resize

## Configuration

Settings are in `%LOCALAPPDATA%\LiteZones\settings.json`. Edit the file or use the tray menu **Settings...** dialog. Changes apply instantly.

## Uninstall

1. Exit LiteZones from the tray menu
2. Delete the exe
3. Remove `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\LiteZones` if autostart was enabled
4. Optionally delete `%LOCALAPPDATA%\LiteZones\`

## Building

Requires [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++".

```
tools\build.cmd
```

## License

[MIT](LICENSE)
