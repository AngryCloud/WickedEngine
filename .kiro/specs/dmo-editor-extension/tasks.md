# Implementation Tasks

## Tasks

- [x] 1. CMake wiring — add DMO client include path to Editor target
  - [x] 1.1 In `external/WickedEngine/Editor/CMakeLists.txt`, add `target_include_directories(WickedEngineEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../Source)` so that `DMOTravelMapWindow.cpp` can include `Travel/WickedTravelMapEditorTool.h`
  - [x] 1.2 Verify the Editor target still builds cleanly after the include path addition
  - **References**: Design §Build Isolation, Requirement 21.3

- [x] 2. DMOTravelMapWindow — header and skeleton
  - [x] 2.1 Create `Editor/DMOTravelMapWindow.h` with the full class declaration: `Create`, `ResizeLayout`, `Update`, `Render`, all UI widget members, `WickedTravelMapEditorTool m_tool`, filter bools, canvas geometry fields, selection/drag state, and private helpers (`UVToCanvas`, `CanvasToUV`, `HitTest`, `RefreshPropertiesPanel`, `ClearPropertiesPanel`)
  - [x] 2.2 Create `Editor/DMOTravelMapWindow.cpp` with stub implementations of all methods (no-ops that compile cleanly)
  - [x] 2.3 Add `#include "DMOTravelMapWindow.h"` to `Editor.h`, declare `DMOTravelMapWindow dmoTravelMapWnd` and `wi::gui::Button dmoTravelMapButton` as members of `EditorComponent`
  - **References**: Design §DMOTravelMapWindow, Requirements 1.1–1.5

- [x] 3. DMOTravelMapWindow — Create and ResizeLayout
  - [x] 3.1 Implement `Create(EditorComponent* editor)`: store editor pointer, create and configure all `wi::gui::Button`, `wi::gui::CheckBox`, `wi::gui::Label`, `wi::gui::TextInputField`, and `wi::gui::Slider` widgets with correct labels and default states (filter checkboxes default checked)
  - [x] 3.2 Implement `ResizeLayout()`: compute `m_canvasOrigin` and `m_canvasSize` from window bounds, apply aspect-ratio-preserving scale (2163:3336), reflow properties panel and toolbar widgets
  - [x] 3.3 In `EditorComponent::Load()`, call `dmoTravelMapWnd.Create(this)` and add `dmoTravelMapButton` to the toolbar
  - **References**: Design §DMOTravelMapWindow, Requirements 1.1–1.5, 4.4

- [x] 4. DMOTravelMapWindow — map texture and anchor overlay rendering
  - [x] 4.1 Implement `Render()`: load `OracleMapWithDalelandsZone.png` as a `wi::Resource` (lazy-loaded on first render), draw it via `wi::image::Draw` scaled to the aspect-ratio-preserving canvas area
  - [x] 4.2 Render Zone anchors as colored rectangles using `wi::image::Draw` with `params.siz = sizePx * canvasScale`, `params.pos = UVToCanvas(centerUV) - siz/2`; skip if `!m_showZones`
  - [x] 4.3 Render InstancedCity anchors as 8×8 point markers using `wi::image::Draw`; skip if `!m_showCities`
  - [x] 4.4 Render PlaceName anchors as text using `wi::font::Draw` at `UVToCanvas(centerUV)`, font size 10; hidden labels at 40% alpha; skip if `!m_showLabels`
  - [x] 4.5 Render selected anchor with highlight color `XMFLOAT4(1.0f, 0.85f, 0.0f, 1.0f)`
  - [x] 4.6 Display "No layout loaded" placeholder label when `!m_loaded`
  - **References**: Design §Anchor overlay rendering, Requirements 2.1–2.3, 3.1–3.5

- [x] 5. DMOTravelMapWindow — filter toggles
  - [x] 5.1 Wire `zonesCheckBox`, `citiesCheckBox`, `labelsCheckBox` `OnChange` callbacks to set `m_showZones`, `m_showCities`, `m_showLabels` respectively
  - [x] 5.2 Verify that toggling a filter checkbox does not call any mutation method on `m_tool` (no `SetAnchorCenterUV`, `SetLabelDisplayName`, etc.)
  - **References**: Design §Filter State, Requirements 4.1–4.4

- [x] 6. DMOTravelMapWindow — anchor selection and properties panel
  - [x] 6.1 Implement `HitTest(clickPos)`: iterate visible anchors in priority order (PlaceName → InstancedCity → Zone), return `anchorId` of first anchor whose rendered bounds contain `clickPos`, or empty string if none
  - [x] 6.2 In `Update()`, on left mouse button press within `m_canvasOrigin`/`m_canvasSize` bounds: call `HitTest`, set `m_selectedAnchorId`, call `RefreshPropertiesPanel()`; on empty-area click call `ClearPropertiesPanel()`
  - [x] 6.3 Implement `RefreshPropertiesPanel()`: populate `propIdLabel`, `propKindLabel`, `propUVLabel` for all anchor kinds; show/enable `propNameField`, `propVisibleBox`, `propConfSlider` only for PlaceName anchors
  - [x] 6.4 Wire `propNameField` commit → `m_tool.SetLabelDisplayName(m_selectedAnchorId, newName)`
  - [x] 6.5 Wire `propVisibleBox` toggle → `m_tool.SetLabelVisible(m_selectedAnchorId, checked)`
  - [x] 6.6 Wire `propConfSlider` change → `m_tool.SetLabelConfidence(m_selectedAnchorId, value)`
  - **References**: Design §Hit test priority, Requirements 5.1–5.4, 7.1–7.4

- [x] 7. DMOTravelMapWindow — drag-to-reposition
  - [x] 7.1 In `Update()`, on left mouse button press on a selected anchor: set `m_dragging = true`, store `m_dragStartMouse` and `m_dragStartUV`
  - [x] 7.2 While `m_dragging` and mouse is moving: compute new UV via `CanvasToUV(currentMousePos)`, call `m_tool.SetAnchorCenterUV(m_selectedAnchorId, newUV)` each frame
  - [x] 7.3 On left mouse button release: clamp final UV to [0,1]×[0,1], call `m_tool.SetAnchorCenterUV` with clamped value, set `m_dragging = false`, call `RefreshPropertiesPanel()` to update UV display
  - **References**: Design §UV ↔ Canvas conversion, Requirements 6.1–6.4

- [x] 8. DMOTravelMapWindow — Load, Save, Import Labels buttons
  - [x] 8.1 Wire `loadButton` click: open file picker for `*.json`, call `m_tool.LoadLayout(selectedPath)`, set `m_loaded` and `m_layoutPath` on success, display anchor count in `statusLabel`; display "Load failed." on failure
  - [x] 8.2 Wire `saveButton` click: if `!m_loaded` display "No layout loaded."; else call `m_tool.SaveLayout(m_layoutPath)`, display "Saved." on success or "Save failed." on failure
  - [x] 8.3 Wire `importLabelsButton` click: open file picker for `*.json`, call `m_tool.ImportPlaceNames(selectedPath)`, display "Imported N labels." on success or "Import failed: 0 labels imported." if count == 0
  - [x] 8.4 After successful import, refresh the MapCanvas to show newly added PlaceName anchors
  - **References**: Requirements 8.1–8.4, 9.1–9.5

- [x] 9. Play From Here — state, toolbar button, and config persistence
  - [x] 9.1 Add `dmoPlayFromHereButton`, `dmoSpawnMarkerSet`, `dmoSpawnPos`, `dmoSpawnYaw`, `dmoSpawnPitch` to `EditorComponent` in `Editor.h`
  - [x] 9.2 In `EditorComponent::Load()`, add `dmoPlayFromHereButton` to the toolbar; read `dmo.spawn.*` keys from `main->config` and restore spawn state if present
  - [x] 9.3 In `EditorComponent::Update()`, on right-click in 3D viewport: cast pick ray via `wi::scene::Pick`; if valid hit, store position and camera yaw/pitch as spawn marker, write all five config keys, call `main->config.Commit()`
  - [x] 9.4 Render a small axis gizmo at `dmoSpawnPos` during `EditorComponent::Render()` when `dmoSpawnMarkerSet == true`
  - **References**: Design §Play From Here, Requirements 10.1–10.4, 12.1–12.2

- [x] 10. Play From Here — playback activation and keyboard shortcuts
  - [x] 10.1 Wire `dmoPlayFromHereButton` click: if `!dmoSpawnMarkerSet` post status message "No spawn point set. Right-click in viewport to place one." and return; else set player start transform and invoke existing `playButton` click logic
  - [x] 10.2 In `EditorComponent::Update()`, handle F5 key press → same action as `dmoPlayFromHereButton` click
  - [x] 10.3 In `EditorComponent::Update()`, handle F6 key press → same action as existing `stopButton` click
  - **References**: Requirements 11.1–11.4

- [x] 11. DMOAssetPlacerWindow — header, skeleton, and EditorComponent integration
  - [x] 11.1 Create `Editor/DMOAssetPlacerWindow.h` with the full class declaration: `Create`, `ResizeLayout`, `Update`, all UI widget members, `PlacementMode` enum, ghost entity, stroke accumulator, scroll-wheel overrides, and private helpers
  - [x] 11.2 Create `Editor/DMOAssetPlacerWindow.cpp` with stub implementations
  - [x] 11.3 Add `#include "DMOAssetPlacerWindow.h"` to `Editor.h`, declare `DMOAssetPlacerWindow dmoAssetPlacerWnd` and `wi::gui::Button dmoAssetPlacerButton` as members of `EditorComponent`
  - [x] 11.4 In `EditorComponent::Load()`, call `dmoAssetPlacerWnd.Create(this)` and add `dmoAssetPlacerButton` to the toolbar
  - **References**: Design §DMOAssetPlacerWindow, Requirements 13.1–13.4

- [x] 12. DMOAssetPlacerWindow — placement modes and ghost entity
  - [x] 12.1 Implement `Create()`: configure `modeCombo` with "Free", "Grid", "Surface-snap", "Brush" options; configure all sliders with correct ranges and defaults
  - [x] 12.2 Implement `UpdateGhostEntity(worldPos, normal)`: create ghost entity on first call via `wi::scene::Entity_CreateObject`, set `ObjectComponent::color.w = 0.5f`, update transform each frame
  - [x] 12.3 Implement `HideGhostEntity()` and `RemoveGhostEntity()` (remove from scene on window close or Brush mode activation)
  - [x] 12.4 In `Update()`, for Surface-snap mode: call `wi::scene::Pick` each frame, call `UpdateGhostEntity` on valid hit, call `HideGhostEntity` on miss
  - [x] 12.5 In `Update()`, for Grid mode: snap cursor world position via `SnapToGrid`, call `UpdateGhostEntity`
  - [x] 12.6 Implement `SnapToGrid(worldPos, gridSize)`: round X and Z to nearest grid multiple, preserve Y
  - **References**: Design §Ghost entity lifecycle, Requirements 14.1–14.4, 15.1–15.3, 18.1–18.4

- [x] 13. DMOAssetPlacerWindow — placement, randomizers, and scroll wheel
  - [x] 13.1 Implement `ApplyRandomizers(basePos, normal)`: apply uniform random scale in [scaleMin, scaleMax], random Y rotation in [0, rotYRange], random tilt in [0, tiltRange]; clamp scaleMin to scaleMax if scaleMin > scaleMax
  - [x] 13.2 Implement `PlaceInstance(pos, normal, recordHistory)`: load asset, apply randomizers + scroll-wheel overrides, add to scene; if `recordHistory` call `editor->AdvanceHistory()`
  - [x] 13.3 In `Update()`, handle scroll wheel: no modifier → `m_scrollYRotation += ticks * 15.0f`; Shift → `m_scrollHeightOfs += ticks * 0.1f`; Ctrl → `m_scrollScale *= pow(1.1f, ticks)` clamped to [0.01, 10.0]
  - [x] 13.4 Implement `ComputeBrushCount(density, radius)`: return `clamp(floor(density * radius * radius * π), 1, maxCount)`
  - [x] 13.5 Implement `CommitBrushStroke()`: call `editor->AdvanceHistory()` once with all entities in `m_strokeEntities`, then clear `m_strokeEntities`
  - [x] 13.6 In `Update()`, for Brush mode: while left mouse held, call `wi::scene::Pick` for each candidate position, call `PlaceInstance` for valid hits (accumulate in `m_strokeEntities`); on mouse release call `CommitBrushStroke()`
  - **References**: Design §Brush count formula, §Scroll wheel handling, Requirements 16.1–16.4, 17.1–17.3, 19.1–19.3, 20.1–20.3

- [x] 14. Standalone tests — DMOEditorExtensionTests.cpp
  - [x] 14.1 Create `Editor/DMOEditorExtensionTests.cpp` with `TestBrushDensityBounds()`: spot-check `ComputeBrushCount` for density/radius pairs `{0.1,0.5}`, `{1.0,5.0}`, `{5.0,20.0}`, `{10.0,50.0}` — verify `1 ≤ count ≤ floor(d × r² × π)`
  - [x] 14.2 Add `TestSpawnMarkerRoundTrip()`: write five spawn config keys to a `wi::config::File`, read them back, verify absolute error ≤ 1e-4 per component
  - [x] 14.3 Add `TestFilterImmutability()`: load a layout into a `WickedTravelMapEditorTool`, snapshot `GetAnchors()`, toggle filter bools in all combinations, verify `GetAnchors()` returns identical data (no mutation methods called)
  - [x] 14.4 Add a `main()` that runs all three tests and prints pass/fail
  - **References**: Design §Testing Strategy, Properties 2, 4, 5
