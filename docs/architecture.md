# FancyZones Internals — Reference Analysis

Analysis of `powertoys/src/modules/fancyzones/` done while planning LiteZones. All paths relative to that folder. This is the "what we replicate and why" reference.

## Module layout

| Folder | Purpose | LiteZones? |
|---|---|---|
| `FancyZonesLib/` | Core C++ engine (all the real logic) | Port |
| `FancyZones/` | Host exe (`PowerToys.FancyZones.exe`) — installs hooks, drives the lib | Fold into main exe |
| `FancyZonesModuleInterface/` | PowerToys runner DLL (`powertoy_create` → `PowertoyModuleIface`) | Drop (no runner) |
| `editor/FancyZonesEditor/` | WPF layout editor | Deferred / drop |
| `FancyZonesEditorCommon/` | Shared C# data contracts | Drop (C++ side reimplements) |
| `FancyZonesCLI/` | Command-line layout tool | Drop |
| `FancyZonesTests/`, `FancyZones.UITests/`, `FancyZonesEditor.UITests/`, `FancyZones.FuzzTests/` | Tests | Port a few math tests only |

## End-to-end flow

1. Host exe installs `SetWindowsHookEx(WH_KEYBOARD_LL)` and `SetWinEventHook` for `EVENT_SYSTEM_MOVESIZESTART/END`, `EVENT_OBJECT_LOCATIONCHANGE` (only while dragging), `NAMECHANGE`, `UNCLOAKED`, `SHOW`, `CREATE`, `DESTROY` (`FancyZones/FancyZonesApp.cpp:76-120`).
2. Win events are forwarded to the lib as `WM_PRIV_*` messages via `IFancyZones`/`IFancyZonesCallback` COM interfaces (`FancyZonesLib/FancyZones.h`). The lib owns a hidden window `L"SuperFancyZones"` (`FancyZones.cpp:65`).
3. Drag start → `WindowMouseSnap` created (`FancyZones.cpp:479`). During the drag, `GenericKeyHook` (Shift) + `MouseButtonsHook` (right/X/middle) feed `DraggingState` (`shift XOR secondaryMouseState`, respecting the `shiftDrag` setting) to choose selection mode; `ZonesOverlay` (D2D) draws zones; `HighlightedZones` tracks initial + current zone.
4. Release → `WindowMouseSnap::MoveSizeEnd` → `WorkArea::Snap` → `Layout::ZonesFromPoint` → `ZoneIndexSetBitmask` → `LayoutAssignedWindows::Assign` + `SetProp` stamps (`FancyZones_zones`, `FancyZones_zones_max128`, `FancyZones_RestoreSize`) → `SetWindowPos` → `AppZoneHistory` persisted to `app-zone-history.json`.

## Key subsystems

### Work areas & monitors
- `WorkArea` = one per monitor. Static factory `WorkArea::Create`, `Init()` = `InitWindow` (overlay HWND) + `InitLayout`. `Snap()` validates zone indices, assigns, writes history. (`WorkArea.cpp`)
- `WorkAreaConfiguration` maps `HMONITOR` → work area; `nullptr` key = span-across-monitors work area. Cursor/window lookups **prefer the spanning work area** when enabled. (`WorkAreaConfiguration.cpp`)
- `MonitorUtils::OrderMonitors` = topological "blocking" sort by `(top, left)` for a deterministic monitor order. WMI (`ROOT\WMI` `WmiMonitorID`) used for hardware monitor IDs — dropped in v1.

### Layout engine
- `LayoutConfigurator`: `Focus/Rows/Columns/Grid/PriorityGrid/Custom`. 11 predefined PriorityGrid templates with percentages in 1/10000ths (`C_MULTIPLIER`). `Custom` builds from grid (rows/columns percentages + cell-child-map) or canvas (absolute rects). (`LayoutConfigurator.cpp`, `LayoutData.h`)
- `Layout::ZonesFromPoint(pt)` = zone selection; `ZoneSelectPriority` with `ClosestCenter` algorithm. (`Layout.cpp`)
- `Zone` = integer-rect math: `GetZoneArea()`, `IsValid()`, `MAX_NEGATIVE_SPACING = -20`.
- `ZoneIndexSetBitmask`: zone index sets stored in two `SetProp` DWORDs (part1 = indices 0–63, part2 = 64–127).

### Drag & snap
- `WindowMouseSnap`: created at drag start with a snapshot of window properties; `Create()` gates on cursor type / processability / elevation; `MoveSizeStart/Update/End`, `Abort()`.
- `DraggingState`: `m_shift ^ m_secondaryMouseState` with `shiftDrag` setting; middle mouse toggles secondary only when `mouseMiddleClickSpanningMultipleZones`.
- `HighlightedZones`: `m_initialHighlightZone` + `m_highlightZone`; `Update()` honors `selectManyZones` → `GetCombinedZoneRange`.
- `WindowDrag.cpp` is **dead code** (references APIs that don't exist; not in the vcxproj) — do not port.

### Keyboard snap
- `WindowKeyboardSnap`: `SnapHotkeyBasedOnZoneNumber` (with VK_LEFT/VK_RIGHT monitor cycling), `SnapBasedOnPositionOnAnotherMonitor`, `Extend`.
- `ChooseNextZoneByPosition` (`util.cpp`): complex-number math for arrow-key zone navigation (direction vector vs zone-center direction, eccentricity ellipse scoring).

### Overlay rendering (D2D)
- `ZonesOverlay`: `D2D1CreateFactory` + `CreateHwndRenderTarget` + solid-color brushes; `GetAnimationAlpha()`: FadeIn 200 ms / Flash 700 ms; cached `IDWriteFactory` for zone numbers. Rotation preview uses `CreateCompatibleRenderTarget` — dropped.
- `Colors::GetZoneColors()` reads `zoneColor`, `zoneBorderColor`, `zoneHighlightColor`, `zoneNumberColor`, `zoneHighlightOpacity` from settings.

### Window classification
- `FancyZonesWindowProcessing::DefineWindowType` (Processable / SplashScreen / Minimized / ToolWindow / NotVisible / NonRootWindow / NonProcessablePopupWindow / ChildWindow / Excluded / NotCurrentVirtualDesktop). `IsProcessableManually` is looser than `IsProcessableAutomatically` (which also excludes Workspaces-launched windows).
- `FancyZonesWindowProperties`: `SetProp`/`GetProp` property names (see flow above).
- `WindowUtils`: `IsWindowMaximized`, `HasVisibleOwner`, `IsRoot` (`GetAncestor(GA_ROOT)`), `IsExcluded` (uppercased process path + user list), `IsCursorTypeIndicatingSizeEvent`, `DisableRoundCorners`.

### Settings
- `Settings` struct + JSON key IDs (snake_case): `fancyzones_span_zones_across_monitors`, `fancyzones_makeDraggedWindowTransparent`, `fancyzones_allowChildWindowSnap`, `fancyzones_disableRoundCornersOnSnap`, `fancyzones_zoneColor`, `fancyzones_zoneBorderColor`, `fancyzones_zoneHighlightColor`, `fancyzones_zoneNumberColor`, `fancyzones_editor_hotkey`, `fancyzones_windowSwitching`, `fancyzones_nextTab_hotkey`, `fancyzones_prevTab_hotkey`, `fancyzones_monitorRotation_hotkey`, `fancyzones_excluded_apps`, `fancyzones_highlight_opacity`, `fancyzones_showZoneNumber`.
- Defaults: `shiftDrag=true`, `editorHotkey=Win+Ctrl+``, `zoneHighlightColor=#AACDFF`, `zoneBorderColor=#FFFFFF`, `highlightOpacity=50`, `showZoneNumber=true`.
- `Settings.cpp` creates a `FileWatcher` on `settings.json` → `WM_PRIV_SETTINGS_CHANGED`.

### Persistence
- All files under `PTSettingsHelper::get_module_save_folder_location(L"FancyZones")` = `%LOCALAPPDATA%\Microsoft\PowerToys\FancyZones`. Each store is a singleton with a `FileWatcher` posting `WM_PRIV_*_FILE_UPDATE`.
- Files: `settings.json`, `zones-settings.json` (legacy, migrated), `app-zone-history.json`, `layout-hotkeys.json`, `layout-templates.json`, `custom-layouts.json`, `applied-layouts.json`, `default-layouts.json`.
- `FancyZonesDataTypes`: `ZoneSetLayoutType` (Blank, Focus, Columns, Rows, Grid, PriorityGrid, Custom); `CustomLayoutType` (Grid=0, Canvas); `MonitorId`, `WorkAreaId` (monitor + instance + serial + number + virtual desktop); model IDs `c_focusModelId=0xFFFF`, `c_rowsModelId=0xFFFE`, `c_columnsModelId=0xFFFD`, `c_gridModelId=0xFFFC`, `c_priorityGridModelId=0xFFFB`, `c_blankCustomModelId=0xFFFA`.

## Input hooks

| Hook | Type | Purpose |
|---|---|---|
| `GenericKeyHook` | `WH_KEYBOARD_LL` | Shift modifier tracking during drag (disabled when debugging) |
| `MouseButtonsHook` | `WH_MOUSE_LL` | Right/X/middle button → secondary/multi-zone modes |
| `KeyboardInput` | Raw input (`RIDEV_INPUTSINK`) | Hotkeys (quick layout switch, etc.) without hook re-entrancy |

Hooks are installed/uninstalled around a drag only — this keeps idle CPU at ~0%.

## Thread model

- Single UI thread owns the lib hidden window and D2D overlay targets.
- Low-level keyboard/mouse hooks run on the installing thread.
- `OnThreadExecutor` = dpi-unaware worker thread for `GetDpiForMonitor` and WMI queries (needed because WMI/display APIs must not run on the DPI-aware UI thread). Keep the dpi-unaware thread concept in LiteZones for DPI work.

## Minimal file set for LiteZones

Core: `FancyZones.h/.cpp`, `FancyZonesWinHookEventIDs.h/.cpp` · Work areas: `WorkArea`, `WorkAreaConfiguration` · Snap: `WindowMouseSnap`, `WindowKeyboardSnap`, `DraggingState`, `HighlightedZones` · Input: `KeyState.h`, `GenericKeyHook.h`, `MouseButtonsHook`, `KeyboardInput` · Geometry: `Layout`, `LayoutConfigurator`, `Zone`, `ZoneIndexSetBitmask`, `LayoutData`, `LayoutDefaults`, `LayoutAssignedWindows` · Overlay: `ZonesOverlay`, `Colors` · Window: `FancyZonesWindowProcessing`, `FancyZonesWindowProperties`, `WindowUtils` · System: `MonitorUtils`, `VirtualDesktop` (optional), `on_thread_executor` · Settings: `Settings`, `SettingsObserver`, `SettingsConstants`, `ModuleConstants` · Persistence: `FancyZonesData`, `FancyZonesDataTypes`, `JsonHelpers`, plus `AppliedLayouts`, `AppZoneHistory` stores · Utils: `util`, `GuidUtils`.

Droppable: `trace.cpp` telemetry, monitor-rotation preview, Workspaces interop, `EditorParameters`, `WindowDrag.cpp`, all `UNIT_TESTS` branches, FuzzTests/CLI.
