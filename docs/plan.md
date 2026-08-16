# LiteZones — Implementation Plan

**Goal:** Standalone Windows utility replicating FancyZones with absolute minimal RAM/CPU/Disk. Single native C++/Win32 exe, no PowerToys runner, no telemetry, no editor process in v1.

**Source of truth:** `powertoys/src/modules/fancyzones/` — port the proven geometry/layout logic from `FancyZonesLib` but strip all PowerToys coupling (`src/common`: logger, telemetry, GPO, Workspaces, WinRT JSON, editor IPC, winrt/wil deps).

## Tech decisions

| Decision | Choice |
|---|---|
| Language | C++17 / pure Win32 (MSVC, `/MT` static runtime) |
| Process model | Single process, single exe |
| Layout editor (v1) | None — built-in templates + hand-edited JSON |
| Toolchain | Visual Studio 2022 Community, "Desktop development with C++" workload |
| JSON parsing | Small self-contained reader/writer module (zero third-party deps) |
| Storage | `%LOCALAPPDATA%\LiteZones\` |

## Performance budget (explicit targets)

| Metric | Target |
|---|---|
| Idle CPU | ~0% (event-driven; hooks active only during drags) |
| Idle RAM | ~8–12 MB working set |
| Disk | single Release exe < 2 MB, no installer |
| Overlay | Direct2D, rendered only while dragging (200 ms fade-in / 700 ms flash, like FancyZones) |

## Feature scope (v1)

- Drag-to-snap with zones overlay (hold Shift by default; configurable).
- Multiple zone selection (span a window across several zones at once).
- Keyboard snap hotkeys (`Win+Ctrl+Alt+[1-9]`, arrow-key zone navigation).
- Restore window size on unsnap.
- Per-monitor layouts + optional span-across-monitors mode.
- App zone history (remember which zone an app last used; snap on reopen).

## How FancyZones works (what we replicate)

1. **Host app** (`FancyZonesApp.cpp:76-120`) installs `WH_KEYBOARD_LL` + `SetWinEventHook` on `MOVESIZESTART/END` and `OBJECT_LOCATIONCHANGE` (location hook only while dragging).
2. **Drag:** `WindowMouseSnap` (created at `FancyZones.cpp:479`) detects drag start; `DraggingState` decides zone-selection mode (`shift XOR right-button`, using `MouseButtonsHook` + `GenericKeyHook`); `ZonesOverlay` (D2D) highlights zones; `HighlightedZones` tracks initial/current.
3. **Snap:** on `MOVESIZEEND` → `WorkArea::Snap` → `Layout::ZonesFromPoint` (overlap algorithm) → save window props via `SetProp` (`FancyZones_zones`, `FancyZones_RestoreSize`) → `SetWindowPos` → persist to `app-zone-history.json`.
4. **Keyboard:** raw-input hotkey `Win+Ctrl+Alt+[1-9]` → `WindowKeyboardSnap` (zone-number or arrow-key `ChooseNextZoneByPosition` complex-number math).
5. **Geometry:** `LayoutConfigurator` (Focus/Rows/Columns/Grid/PriorityGrid + Custom), `MonitorUtils::OrderMonitors` deterministic monitor order, `Zone` hit-test.

## Port vs. drop

**Port (adapted to pure Win32):** `Layout.cpp`, `LayoutConfigurator.cpp`, `Zone.cpp`, `ZoneIndexSetBitmask`, `LayoutData`, `util.cpp` (`OrderMonitors`, `ChooseNextZoneByPosition`, `InitRGB`), `WorkArea.cpp`/`WorkAreaConfiguration` (Snap), `WindowMouseSnap`, `DraggingState`, `HighlightedZones`, `ZonesOverlay` (D2D), `WindowKeyboardSnap`, `WindowUtils` (processability, restore-size), `FancyZonesWindowProperties` (SetProp stamping), `FancyZonesDataTypes` (JSON schema shapes), `Settings` (trimmed), `MonitorUtils` (minus WMI hardware-ID path for v1).

**Drop entirely:** `trace.cpp`/telemetry, GPO checks, Workspaces interop, editor IPC (`EditorParameters`, `WM_PRIV_EDITOR`), monitor-rotation preview, `WindowDrag.cpp` (dead code), FuzzTests/CLI/UITests, virtual-desktop keying in v1 (zone history keyed by work area only).

## Project layout

```
LiteZones/
├─ LiteZones.sln
├─ docs/
│  ├─ plan.md            # this document
│  ├─ architecture.md    # FancyZones internals analysis
│  └─ config.md          # JSON config formats + storage paths
├─ src/litezones/
│  ├─ LiteZones.cpp/h      # wWinMain, message loop, single-instance mutex
│  ├─ App.cpp/h            # tray icon, autostart (HKCU Run), Enable/Disable, Reload, Exit
│  ├─ Hooks.cpp/h          # WH_KEYBOARD_LL + SetWinEventHook wrappers (drag-scoped)
│  ├─ MonitorManager.cpp/h # EnumDisplayMonitors + OrderMonitors, span-across-monitors
│  ├─ WorkArea.cpp/h       # per-monitor layout + Snap()
│  ├─ LayoutEngine.cpp/h   # LayoutConfigurator port (templates + custom grid/canvas)
│  ├─ Zone.cpp/h           # zone rect math, hit-test, closest-center
│  ├─ DragController.cpp/h # WindowMouseSnap port + DraggingState (XOR modes)
│  ├─ KeyboardSnap.cpp/h   # Win+Ctrl+Alt+num, arrow-key zone nav
│  ├─ ZonesOverlay.cpp/h   # D2D topmost overlay (on-demand)
│  ├─ WindowUtils.cpp/h    # processability, restore-size, SetProp stamping
│  ├─ Settings.cpp/h       # settings.json + ReadDirectoryChangesW file watcher
│  ├─ Persistence.cpp/h    # custom/applied/zone-history JSON stores
│  └─ json.{h,cpp}         # minimal JSON module
├─ tools/build.cmd         # MSBuild Release|x64
└─ README.md               # config format + hotkeys + autostart docs
```

## Milestones

1. **M0 — Scaffold:** install VS; create solution; exe with hidden window, tray icon, single-instance mutex, clean build via `tools/build.cmd`.
2. **M1 — Settings + persistence:** `settings.json` loader (shift-drag, hotkeys, colors, span flag, restore-size); data stores with file watcher; tray menu wired.
3. **M2 — Monitor/layout engine:** ordered work areas, per-monitor layouts, span mode; port `LayoutConfigurator` templates + `Zone` math; defaults on first run. Validate by porting a few `FancyZonesTests/UnitTests/Zone.Spec.cpp` / `Layout.Spec.cpp` cases into a small test exe.
4. **M3 — Drag-to-snap (core):** hooks, `DragController`, overlay, multi-zone selection, snap + restore-size, elevated/no-border window handling. **Measure idle RAM/CPU here.**
5. **M4 — Keyboard snap + zone history:** `Win+Ctrl+Alt+[1-9]`/arrow nav; remember app's last zone and snap on reopen; apply layouts on monitor add/remove.
6. **M5 — Custom layouts via JSON:** grid (rows/columns percentages + cell-child-map) and canvas (absolute rects) in `custom-layouts.json`; tray "cycle layout on monitor" as editor substitute.
7. **M6 — Packaging + perf verification:** Release `/MT` build, verify <2 MB / idle targets with Process Explorer; autostart registration + simple uninstall (delete folder + run key); README.

## Open design note

Without an editor, "which layout applies to which monitor" is configured in `applied-layouts.json` by monitor + layout reference (templates by type, custom by UUID), plus a tray action to cycle layouts on the active monitor for quick experimentation.

## Build/verify

`tools/build.cmd` (msbuild Release x64) → run `LiteZones.exe` → manual test matrix (drag, keyboard snap, multi-select, span, restore-size, per-monitor layouts, monitor hot-plug) + RAM/CPU measurement.
