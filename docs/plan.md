# LiteZones — Implementation Plan

**Goal:** Standalone Windows utility replicating FancyZones with absolute minimal RAM/CPU/Disk. Single native C++/Win32 exe, no PowerToys runner, no telemetry, no editor process in v1.

**Source of truth:** `powertoys/src/modules/fancyzones/` — port the proven geometry/layout logic from `FancyZonesLib` but strip all PowerToys coupling (`src/common`: logger, telemetry, GPO, Workspaces, WinRT JSON, editor IPC, winrt/wil deps).

## Tech decisions

| Decision | Choice |
|---|---|
| Language | C++17 / pure Win32 (MSVC, `/MT` static runtime) |
| Process model | Single process, single exe |
| Layout editor (v1) | In-process GUI editor window (M5): grid + canvas editors, per-monitor apply + preview |
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
- In-process layout editor: create/edit grid and canvas layouts, apply per monitor (M5).

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
│  ├─ Settings.cpp/h       # settings.json (17-key camelCase) + first-run defaults
│  ├─ FileWatcher.cpp/h    # ReadDirectoryChangesW -> PostMessage(WM_APP+2) hot reload
│  ├─ Persistence.cpp/h    # custom/applied/zone-history JSON stores (M2/M4)
│  └─ json.{h,cpp}         # minimal JSON module
├─ tools/build.cmd         # MSBuild Release|x64
└─ README.md               # config format + hotkeys + autostart docs
```

## Milestones

1. **M0 — Scaffold:** install VS; create solution; exe with hidden window, tray icon, single-instance mutex, clean build via `tools/build.cmd`. **DONE** — Release exe 118 KB, idle ~9.6 MB RAM, mutex verified.
2. **M1 — Settings + persistence:** `settings.json` loader (shift-drag, hotkeys, colors, span flag, restore-size); data stores with file watcher; tray menu wired. **DONE** — `json`, `Paths`, `Settings`, `FileWatcher` modules; hot reload via `WM_APP+2`; "Reload config" + "Open config folder" in tray; Release exe 223 KB, idle ~9.4 MB. Persistence stores (`Persistence.cpp`) folded into M2/M4 rather than stubbed.
3. **M2 — Monitor/layout engine:** ordered work areas, per-monitor layouts, span mode; port `LayoutConfigurator` templates + `Zone` math; defaults on first run. Validate by porting a few `FancyZonesTests/UnitTests/Zone.Spec.cpp` / `Layout.Spec.cpp` cases into a small test exe. **DONE** — new modules `Zone`, `LayoutTypes`, `LayoutEngine` (Focus/Rows/Columns/Grid/PriorityGrid/Custom + `CalculateGridZones` + `Layout` + selection algorithms + `SetCustomLayoutData` registry), `MonitorManager` (ordered work areas, DPI), `WorkArea`, `WorkAreaManager`; app reloads work areas on `WM_DISPLAYCHANGE` and config reload (default PriorityGrid/3). New `tests/ZoneTests` console project: 16 ported tests all green (`bin\x64\Release\ZoneTests.exe`, exit 0). Release exe 246 KB, idle 9.5 MB WS.
4. **M3 — Drag-to-snap (core):** hooks, `DragController`, overlay, multi-zone selection, snap + restore-size, elevated/no-border window handling. **DONE** — new modules `Hooks` (WinEvent MOVESIZESTART/MOVESIZEEND/DESTROY + drag-scoped LOCATIONCHANGE, WH_KEYBOARD_LL Shift/Ctrl, WH_MOUSE_LL right/middle toggles → WM_PRIV_*), `WindowUtils` (save/restore size+origin, SizeWindowToRect, ScreenToWorkAreaCoords, transparency, cursor-shape resize guard), `WindowProcessing` (IsProcessableManually), `WindowProperties` (ZoneIndexSetBitmask SetProp stamping), `LayoutAssignedWindows`, `HighlightedZones`, `ZonesOverlay` (Direct2D), `Colors` (GetZoneColors), `DragController` (DraggingState XOR modes + WindowMouseSnap port). `WorkArea` gained a topmost popup tool window + Snap/Unsnap/ShowZones/HideZones. Verified: 17/17 unit tests, exe 287.5 KB, idle 10.3 MB WS / 1.7 MB private, manual launch OK.
5. **M4 — Keyboard snap + zone history:** `Win+Ctrl+Alt+[1-9]`/arrow nav; remember app's last zone and snap on reopen; apply layouts on monitor add/remove. **DONE** — new modules `util` (`ChooseNextZoneByPosition`/`PrepareRectForCycling`), `AppZoneHistory` (`app-zone-history.json` keyed by app process path only), `KeyboardSnap` (zone-number snap, arrow nav by index or position, cross-monitor cycling); `Hooks` gained `WM_PRIV_SNAP_HOTKEY`/`WM_PRIV_WINDOWCREATED` (WM_APP+16/17), snap-hotkey swallowing in the keyboard proc (Win+Ctrl+Alt+digit/arrow, plus Win+arrow when `overrideSnapHotkeys`), and EVENT_OBJECT_SHOW/CREATE window-created events gated on `snapToAppZoneOnOpen`; `WorkArea::Snap` persists history; `WindowUtils::GetProcessPath` / `LayoutAssignedWindows::GetZoneIndexSetFromWindow` added. Verified: 19/19 unit tests (TestChooseNextZoneByPosition, TestAppZoneHistoryStore), exe 314.5 KB, idle 10.3 MB WS / 1.7 MB private, smoke launch OK.
6. **M5 — Layout editor (in-process):** phases — (P1) `CustomLayouts`/`AppliedLayouts` JSON stores + stable monitor device-id key + per-monitor layout resolution in `WorkAreaManager::Update` + tray "Cycle layout on monitor" + file watcher; (P2) editor window shell + layout library list + zone preview canvas (new `EditorWindow`, `EditorCanvas`, New/Duplicate/Delete/Rename dialogs); (P3) grid editor (drag row/column separators to resize, split via double-click, select + merge via context menu — `GridData` port of `GridData.cs` in Multiplier space); (P4) canvas editor (pen-draw zones, move/resize with handles, delete via Delete key/context menu); (P5) per-monitor Apply + Apply-to-all + monitor-aspect preview (work-area rect fed into template/grid preview; combo `CBN_SELCHANGE` re-renders). **DONE** — 29 unit-test suites green in Release+Debug; editor runtime-verified end-to-end (draw/move/resize/delete persisted to `custom-layouts.json` on close; Apply + Apply-to-all write `applied-layouts.json` with correct type/uuid; app stays alive throughout). `config.md` examples corrected (cell-child-map shape, device interface-string key).
7. **M6 — Packaging + perf verification:** Release `/MT` build, verify <2 MB / idle targets with Process Explorer; autostart registration + simple uninstall (delete folder + run key); README.

## Open design note

The layout editor is a window inside LiteZones.exe (no second process, no IPC): it reads/writes the same `custom-layouts.json` / `applied-layouts.json` stores the runtime uses, and a config-file watch triggers the work-area rebuild. "Which layout applies to which monitor" is stored per monitor by a stable device-id key derived from `EnumDisplayDevices` (no WMI, no virtual-desktop keying in v1).

## Build/verify

`tools/build.cmd` (msbuild Release x64) → run `LiteZones.exe` → manual test matrix (drag, keyboard snap, multi-select, span, restore-size, per-monitor layouts, monitor hot-plug) + RAM/CPU measurement.
