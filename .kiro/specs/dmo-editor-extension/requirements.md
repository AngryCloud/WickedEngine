# Requirements Document

## Introduction

The DMO Editor Extension adds three DMO-specific authoring tools to the Wicked Engine Editor fork. These tools live exclusively in the editor fork (`external/WickedEngine/`) and are never compiled into production client builds. Each tool follows the established `wi::gui::Window` pattern used by `PaintToolWindow` and `TerrainWindow`, integrating into `EditorComponent` via a `Create(EditorComponent* editor)` method.

The three work packets, in priority order, are:

1. **Travel Map Editor Window** — a 2D map authoring panel wrapping `WickedTravelMapEditorTool` for editing zone anchors, city pins, and place-name labels on the Oracle map texture.
2. **Play From Here** — a toolbar button and keyboard shortcut that starts scene playback from a user-defined spawn point.
3. **Asset Placer Tool** — a surface-snap placement window with brush, grid, and randomizer modes layered on top of the existing `ContentBrowserWindow`.

---

## Glossary

- **DMOTravelMapWindow**: The new `wi::gui::Window` subclass that hosts the 2D travel map editor UI.
- **WickedTravelMapEditorTool**: The existing pure data-layer C++ class (`Source/Travel/WickedTravelMapEditorTool.h`) that owns the loaded `WickedZoneMapLayout` and exposes load/save/mutate operations. No GPU resources.
- **WickedZoneMapLayout**: The in-memory representation of the full map document — zones, instanced cities, and place-name labels — as defined in `WickedTravelMapTypes.h`.
- **WickedZoneMapAnchorRecord**: A single anchor entry (zone box, city pin, or place-name label) with `anchorId`, `displayName`, `kind`, `centerUV`, `sizePx`, `confidence`, and `notes` fields.
- **WickedZoneMapAnchorKind**: Enum with values `Zone`, `InstancedCity`, and `PlaceName`.
- **AnchorKind**: Shorthand for `WickedZoneMapAnchorKind` when used in UI context.
- **MapCanvas**: The 2D region inside `DMOTravelMapWindow` where the map texture and overlays are rendered using `wi::image::Draw` and `wi::font::Draw`.
- **PropertiesPanel**: The side panel within `DMOTravelMapWindow` that displays and allows editing of the currently selected anchor's fields.
- **SpawnMarker**: The world-space position and rotation stored by the Play From Here feature, representing where playback begins.
- **DMOAssetPlacerWindow**: The new `wi::gui::Window` subclass that provides surface-snap, grid, brush, and free placement modes.
- **GhostEntity**: A temporary scene entity rendered at the cursor position as a placement preview in `DMOAssetPlacerWindow`.
- **EditorComponent**: The existing `wi::RenderPath2D` subclass in `Editor.h` that owns all editor windows and toolbar buttons.
- **AdvanceHistory**: The existing `EditorComponent` method that records an undoable operation into the editor's history stack.
- **wi::config::File**: The Wicked Engine config file abstraction used by the `Editor` class for persistent settings.
- **OCR JSON**: A `place-name-records.json` file in pixel-coordinate format produced by the OCR pipeline, consumed by `WickedTravelMapEditorTool::ImportPlaceNames`.
- **zone-map-layout-v1.json**: The authoritative map document on disk, read and written by `WickedZoneMapLayoutLoader`.

---

## Requirements

### Requirement 1: DMOTravelMapWindow Integration

**User Story:** As a map author, I want a dockable Travel Map Editor panel in the Wicked Editor, so that I can visually position zone anchors, city pins, and place-name labels on the Oracle map texture without leaving the editor.

#### Acceptance Criteria

1. THE `DMOTravelMapWindow` SHALL inherit from `wi::gui::Window` and expose a `Create(EditorComponent* editor)` method matching the pattern of `PaintToolWindow` and `TerrainWindow`.
2. THE `EditorComponent` SHALL declare a `DMOTravelMapWindow dmoTravelMapWnd` member in `Editor.h` and register it during `EditorComponent::Load()`.
3. THE `DMOTravelMapWindow` SHALL store a pointer to `EditorComponent` and a `WickedTravelMapEditorTool` instance as private members.
4. WHEN `DMOTravelMapWindow` is created, THE `DMOTravelMapWindow` SHALL NOT load any layout file automatically; the user initiates loading via the Load button.
5. THE `DMOTravelMapWindow` SHALL implement `ResizeLayout()` to reflow its MapCanvas and PropertiesPanel when the window is resized.

---

### Requirement 2: Map Texture Rendering

**User Story:** As a map author, I want to see the Oracle map texture rendered as a 2D background inside the Travel Map Editor panel, so that I have a visual reference for anchor placement.

#### Acceptance Criteria

1. WHEN the `DMOTravelMapWindow` is visible and a layout is loaded, THE `DMOTravelMapWindow` SHALL render `OracleMapWithDalelandsZone.png` as a 2D background in the MapCanvas using `wi::image::Draw`.
2. THE `DMOTravelMapWindow` SHALL scale the map texture to fill the available MapCanvas area while preserving the source aspect ratio (2163 × 3336).
3. WHEN no layout is loaded, THE `DMOTravelMapWindow` SHALL display a placeholder label reading "No layout loaded" in the MapCanvas area.

---

### Requirement 3: Anchor Overlay Rendering

**User Story:** As a map author, I want zone boxes, city pins, and place-name labels drawn on top of the map texture, so that I can see where each anchor is positioned.

#### Acceptance Criteria

1. WHEN a layout is loaded and zone anchors are visible, THE `DMOTravelMapWindow` SHALL draw each `Zone` anchor as a colored rectangle overlay on the MapCanvas using `wi::image::Draw`, positioned and sized according to `centerUV` and `sizePx` scaled to canvas coordinates.
2. WHEN a layout is loaded and city anchors are visible, THE `DMOTravelMapWindow` SHALL draw each `InstancedCity` anchor as a point marker (filled circle or cross icon) at the `centerUV` position scaled to canvas coordinates.
3. WHEN a layout is loaded and label anchors are visible, THE `DMOTravelMapWindow` SHALL draw each `PlaceName` anchor's `displayName` as text at the `centerUV` position using `wi::font::Draw`.
4. WHEN an anchor is selected, THE `DMOTravelMapWindow` SHALL render that anchor's overlay with a distinct highlight color (yellow, RGBA 1.0, 0.85, 0.0, 1.0) to distinguish it from unselected anchors.
5. WHEN a `PlaceName` anchor has `IsLabelVisible` returning false, THE `DMOTravelMapWindow` SHALL render that label's text at 40% opacity to indicate its hidden state without removing it from the canvas.

---

### Requirement 4: Filter Toggles

**User Story:** As a map author, I want to independently show or hide zones, cities, and labels on the map canvas, so that I can focus on one anchor kind at a time without modifying the underlying data.

#### Acceptance Criteria

1. THE `DMOTravelMapWindow` SHALL provide three `wi::gui::CheckBox` controls labelled "Zones", "Cities", and "Labels" that independently control visibility of each `WickedZoneMapAnchorKind` in the MapCanvas.
2. WHEN a filter checkbox is unchecked, THE `DMOTravelMapWindow` SHALL omit all anchors of the corresponding kind from MapCanvas rendering without calling any mutation method on `WickedTravelMapEditorTool`.
3. WHEN a filter checkbox is re-checked, THE `DMOTravelMapWindow` SHALL immediately restore rendering of the corresponding anchor kind using the current in-memory data from `WickedTravelMapEditorTool::GetAnchors()`.
4. THE filter checkboxes SHALL default to checked (all anchor kinds visible) when the window is created.

---

### Requirement 5: Anchor Selection

**User Story:** As a map author, I want to click on an anchor in the map canvas to select it and see its properties, so that I can inspect and edit individual anchors.

#### Acceptance Criteria

1. WHEN the user clicks within the MapCanvas, THE `DMOTravelMapWindow` SHALL perform a hit test against all currently visible anchors and select the anchor whose rendered bounds contain the click position.
2. WHEN multiple anchors overlap at the click position, THE `DMOTravelMapWindow` SHALL select the topmost anchor in render order (labels above cities above zones).
3. WHEN an anchor is selected, THE `DMOTravelMapWindow` SHALL populate the PropertiesPanel with that anchor's `anchorId`, `displayName`, `kind`, `centerUV`, `confidence`, and visibility state.
4. WHEN the user clicks on an empty area of the MapCanvas, THE `DMOTravelMapWindow` SHALL deselect the current anchor and clear the PropertiesPanel.

---

### Requirement 6: Anchor Drag-to-Reposition

**User Story:** As a map author, I want to drag a selected anchor to reposition it on the map, so that I can correct anchor placements interactively.

#### Acceptance Criteria

1. WHEN the user presses the left mouse button on a selected anchor and moves the mouse within the MapCanvas, THE `DMOTravelMapWindow` SHALL call `WickedTravelMapEditorTool::SetAnchorCenterUV` with the new UV coordinates derived from the mouse position relative to the MapCanvas bounds.
2. WHEN a drag operation is in progress, THE `DMOTravelMapWindow` SHALL update the anchor's rendered position in real time on each frame.
3. WHEN the user releases the left mouse button after a drag, THE `DMOTravelMapWindow` SHALL clamp the final `centerUV` to [0.0, 1.0] × [0.0, 1.0] before committing via `SetAnchorCenterUV`.
4. WHEN a drag operation completes, THE `DMOTravelMapWindow` SHALL update the PropertiesPanel `centerUV` display to reflect the new position.

---

### Requirement 7: Properties Panel Editing

**User Story:** As a map author, I want to edit an anchor's display name, visibility, and confidence directly in the properties panel, so that I can refine label metadata without switching tools.

#### Acceptance Criteria

1. WHEN a `PlaceName` anchor is selected, THE PropertiesPanel SHALL display a `wi::gui::TextInputField` pre-populated with the anchor's `displayName`, and committing the field SHALL call `WickedTravelMapEditorTool::SetLabelDisplayName`.
2. WHEN a `PlaceName` anchor is selected, THE PropertiesPanel SHALL display a `wi::gui::CheckBox` reflecting `WickedTravelMapEditorTool::IsLabelVisible`, and toggling it SHALL call `WickedTravelMapEditorTool::SetLabelVisible`.
3. WHEN a `PlaceName` anchor is selected, THE PropertiesPanel SHALL display a `wi::gui::Slider` in the range [0.0, 1.0] reflecting the anchor's `confidence`, and adjusting it SHALL call `WickedTravelMapEditorTool::SetLabelConfidence`.
4. WHEN a `Zone` or `InstancedCity` anchor is selected, THE PropertiesPanel SHALL display the anchor's `anchorId`, `displayName`, `centerUV`, and `confidence` as read-only labels.

---

### Requirement 8: Import Labels

**User Story:** As a map author, I want to import place-name labels from an OCR JSON file, so that I can seed the label set from automated OCR output and then refine positions manually.

#### Acceptance Criteria

1. THE `DMOTravelMapWindow` SHALL provide a `wi::gui::Button` labelled "Import Labels" that opens a file picker for selecting a `place-name-records.json` file.
2. WHEN a valid OCR JSON file is selected, THE `DMOTravelMapWindow` SHALL call `WickedTravelMapEditorTool::ImportPlaceNames` with the selected file path and display the count of imported labels in a status label.
3. IF `ImportPlaceNames` returns 0, THEN THE `DMOTravelMapWindow` SHALL display an error message "Import failed: 0 labels imported" in the status label without modifying the MapCanvas.
4. WHEN labels are successfully imported, THE `DMOTravelMapWindow` SHALL refresh the MapCanvas to display the newly added `PlaceName` anchors.

---

### Requirement 9: Save and Load Layout

**User Story:** As a map author, I want Save and Load buttons in the Travel Map Editor, so that I can persist my edits to `zone-map-layout-v1.json` and reload from disk when needed.

#### Acceptance Criteria

1. THE `DMOTravelMapWindow` SHALL provide a `wi::gui::Button` labelled "Save" that calls `WickedTravelMapEditorTool::SaveLayout` with the currently configured JSON path and displays "Saved." in the status label on success.
2. IF `SaveLayout` returns false, THEN THE `DMOTravelMapWindow` SHALL display "Save failed." in the status label and leave the in-memory layout unmodified.
3. THE `DMOTravelMapWindow` SHALL provide a `wi::gui::Button` labelled "Load" that opens a file picker for selecting a `zone-map-layout-v1.json` file and calls `WickedTravelMapEditorTool::LoadLayout`.
4. WHEN `LoadLayout` succeeds, THE `DMOTravelMapWindow` SHALL refresh the MapCanvas and display the total anchor count in the status label.
5. IF `LoadLayout` returns false, THEN THE `DMOTravelMapWindow` SHALL display "Load failed." in the status label and leave the MapCanvas in its previous state.

---

### Requirement 10: Play From Here — Spawn Marker

**User Story:** As a developer, I want to right-click in the 3D viewport to set a spawn marker, so that I can define where scene playback begins without editing scene data.

#### Acceptance Criteria

1. WHEN the user right-clicks in the 3D viewport while the Play From Here mode is active, THE `EditorComponent` SHALL store the world-space position and rotation derived from the pick ray intersection with scene geometry as the `SpawnMarker`.
2. WHEN a `SpawnMarker` is set, THE `EditorComponent` SHALL render a small axis gizmo at the `SpawnMarker` world position during the editor's `Render` pass.
3. THE `SpawnMarker` world position and rotation SHALL be persisted to `wi::config::File` under the keys `dmo.spawn.x`, `dmo.spawn.y`, `dmo.spawn.z`, `dmo.spawn.yaw`, and `dmo.spawn.pitch` on each update.
4. WHEN the editor starts, THE `EditorComponent` SHALL read the `SpawnMarker` from `wi::config::File` if the keys are present, restoring the last-set spawn position.

---

### Requirement 11: Play From Here — Playback

**User Story:** As a developer, I want a "Play From Here" toolbar button and F5 keyboard shortcut that starts scene playback from the spawn marker, so that I can iterate on gameplay from a specific location quickly.

#### Acceptance Criteria

1. THE `EditorComponent` SHALL add a `wi::gui::Button` labelled "▶ Play From Here" to the toolbar that, when clicked, sets the player start transform to the current `SpawnMarker` and triggers the existing `playButton` logic.
2. WHEN the F5 key is pressed and a `SpawnMarker` is set, THE `EditorComponent` SHALL execute the same action as clicking "▶ Play From Here".
3. WHEN the F6 key is pressed during playback, THE `EditorComponent` SHALL execute the same action as clicking the existing `stopButton`.
4. WHEN "▶ Play From Here" is activated and no `SpawnMarker` has been set, THE `EditorComponent` SHALL display a status message "No spawn point set. Right-click in viewport to place one." and SHALL NOT start playback.

---

### Requirement 12: Play From Here — Spawn Position Round-Trip

**User Story:** As a developer, I want the spawn position to survive editor restarts, so that I do not have to re-place the spawn marker every session.

#### Acceptance Criteria

1. WHEN the editor saves its config via `wi::config::File::Commit`, THE `EditorComponent` SHALL write the current `SpawnMarker` position and rotation to the config file.
2. WHEN the editor loads its config on startup, THE `EditorComponent` SHALL reconstruct the `SpawnMarker` from the saved config values such that the restored position equals the saved position within floating-point precision (absolute error ≤ 1e-4 per component).

---

### Requirement 13: DMOAssetPlacerWindow Integration

**User Story:** As a level designer, I want a dedicated Asset Placer panel in the Wicked Editor, so that I can place scene assets with surface-snap, grid, brush, and free placement modes.

#### Acceptance Criteria

1. THE `DMOAssetPlacerWindow` SHALL inherit from `wi::gui::Window` and expose a `Create(EditorComponent* editor)` method.
2. THE `EditorComponent` SHALL declare a `DMOAssetPlacerWindow dmoAssetPlacerWnd` member in `Editor.h` and register it during `EditorComponent::Load()`.
3. THE `DMOAssetPlacerWindow` SHALL provide a `wi::gui::ComboBox` with four placement mode options: "Free", "Grid", "Surface-snap", and "Brush".
4. THE `DMOAssetPlacerWindow` SHALL store a pointer to `EditorComponent` as a private member.

---

### Requirement 14: Asset Placer — Surface-Snap Mode

**User Story:** As a level designer, I want placed assets to snap to scene geometry and align to the surface normal, so that objects sit naturally on terrain and meshes.

#### Acceptance Criteria

1. WHEN Surface-snap mode is active and the cursor is over the 3D viewport, THE `DMOAssetPlacerWindow` SHALL call `wi::scene::Pick` each frame to find the scene geometry intersection point and surface normal under the cursor.
2. WHEN a valid pick result is found, THE `DMOAssetPlacerWindow` SHALL position the `GhostEntity` at the pick intersection point and orient it so that its local Y-axis aligns with the surface normal.
3. WHEN the user left-clicks in Surface-snap mode, THE `DMOAssetPlacerWindow` SHALL instantiate the selected asset at the `GhostEntity`'s current transform and record the placement in the editor history via `EditorComponent::AdvanceHistory`.
4. IF no valid pick result is found (cursor over empty space), THEN THE `DMOAssetPlacerWindow` SHALL hide the `GhostEntity` and SHALL NOT place an asset on click.

---

### Requirement 15: Asset Placer — Grid Mode

**User Story:** As a level designer, I want placed assets to snap to a configurable grid, so that I can create evenly spaced layouts.

#### Acceptance Criteria

1. WHEN Grid mode is active, THE `DMOAssetPlacerWindow` SHALL provide a `wi::gui::Slider` labelled "Grid Size" in the range [0.1, 100.0] meters that controls the snap interval.
2. WHEN Grid mode is active and the cursor is over the 3D viewport, THE `DMOAssetPlacerWindow` SHALL snap the `GhostEntity` position to the nearest grid point by rounding each world-space X and Z coordinate to the nearest multiple of the grid size.
3. WHEN the user left-clicks in Grid mode, THE `DMOAssetPlacerWindow` SHALL instantiate the selected asset at the snapped position and record the placement via `EditorComponent::AdvanceHistory`.

---

### Requirement 16: Asset Placer — Brush Mode

**User Story:** As a level designer, I want to paint multiple asset instances in a radius with density control, so that I can quickly populate areas with foliage or debris.

#### Acceptance Criteria

1. WHEN Brush mode is active, THE `DMOAssetPlacerWindow` SHALL provide a `wi::gui::Slider` labelled "Radius" in the range [0.5, 50.0] meters and a `wi::gui::Slider` labelled "Density" in the range [0.1, 10.0] instances per square meter.
2. WHEN the user holds the left mouse button in Brush mode, THE `DMOAssetPlacerWindow` SHALL place asset instances within the brush radius at positions sampled from a uniform random distribution, at a rate such that the total instance count per stroke is at least 1 and at most `floor(density × radius² × π)`.
3. WHEN Brush mode places instances, THE `DMOAssetPlacerWindow` SHALL call `wi::scene::Pick` for each candidate position to snap instances to scene geometry, discarding candidates with no valid pick result.
4. WHEN a brush stroke completes (mouse button released), THE `DMOAssetPlacerWindow` SHALL record all placements from that stroke as a single undoable operation via `EditorComponent::AdvanceHistory`.

---

### Requirement 17: Asset Placer — Randomizers

**User Story:** As a level designer, I want scale, rotation, and tilt randomizers applied to each placed instance, so that placed assets look natural rather than uniform.

#### Acceptance Criteria

1. THE `DMOAssetPlacerWindow` SHALL provide `wi::gui::Slider` controls for "Scale Min" and "Scale Max" in the range [0.01, 10.0], "Rotation Y Range" in the range [0.0, 360.0] degrees, and "Tilt Range" in the range [0.0, 45.0] degrees.
2. WHEN an asset is placed (any mode), THE `DMOAssetPlacerWindow` SHALL apply a uniform random scale in [Scale Min, Scale Max], a uniform random Y rotation in [0, Rotation Y Range], and a uniform random tilt (rotation around a random horizontal axis) in [0, Tilt Range] to the placed instance's transform.
3. WHEN Scale Min exceeds Scale Max, THE `DMOAssetPlacerWindow` SHALL clamp Scale Min to equal Scale Max before applying the randomizer.

---

### Requirement 18: Asset Placer — Ghost Preview

**User Story:** As a level designer, I want to see a ghost preview of the asset at the cursor position before placing it, so that I can verify placement before committing.

#### Acceptance Criteria

1. WHEN an asset is selected in `DMOAssetPlacerWindow` and the cursor is over the 3D viewport, THE `DMOAssetPlacerWindow` SHALL maintain a single `GhostEntity` in the current scene rendered at the cursor's resolved placement position.
2. THE `GhostEntity` SHALL be rendered with 50% opacity to distinguish it from placed instances.
3. WHEN the cursor leaves the 3D viewport or no asset is selected, THE `DMOAssetPlacerWindow` SHALL hide the `GhostEntity`.
4. WHEN the window is closed or the mode changes to Brush, THE `DMOAssetPlacerWindow` SHALL remove the `GhostEntity` from the scene.

---

### Requirement 19: Asset Placer — Scroll Wheel Controls

**User Story:** As a level designer, I want scroll wheel shortcuts to adjust height offset, scale, and Y rotation while hovering over the viewport, so that I can fine-tune placement without switching to the properties panel.

#### Acceptance Criteria

1. WHEN the cursor is over the 3D viewport and the user scrolls the mouse wheel without modifier keys, THE `DMOAssetPlacerWindow` SHALL rotate the `GhostEntity` around its local Y-axis by 15 degrees per scroll tick.
2. WHEN the cursor is over the 3D viewport and the user scrolls the mouse wheel while holding Shift, THE `DMOAssetPlacerWindow` SHALL offset the `GhostEntity` height (Y position) by 0.1 meters per scroll tick.
3. WHEN the cursor is over the 3D viewport and the user scrolls the mouse wheel while holding Ctrl, THE `DMOAssetPlacerWindow` SHALL scale the `GhostEntity` uniformly by a factor of 1.1 per upward tick and 1/1.1 per downward tick, clamped to [0.01, 10.0].

---

### Requirement 20: Asset Placer — Undo Support

**User Story:** As a level designer, I want each placement operation to be undoable via the editor's standard Ctrl+Z, so that I can recover from accidental placements.

#### Acceptance Criteria

1. WHEN a single asset is placed (Free, Grid, or Surface-snap mode), THE `DMOAssetPlacerWindow` SHALL record the placement as one entry in the editor history via `EditorComponent::AdvanceHistory`.
2. WHEN a brush stroke completes, THE `DMOAssetPlacerWindow` SHALL record all instances placed during that stroke as a single history entry via `EditorComponent::AdvanceHistory`.
3. WHEN the user invokes undo via `EditorComponent::ConsumeHistoryOperation`, THE `EditorComponent` SHALL remove all entities added by the corresponding placement operation from the current scene.

---

### Requirement 21: Build Isolation

**User Story:** As a build engineer, I want the DMO editor extension files to be compiled only when building the Wicked Engine Editor target, so that no editor-only code is ever included in production client builds.

#### Acceptance Criteria

1. THE `DMOTravelMapWindow` source files (`DMOTravelMapWindow.h`, `DMOTravelMapWindow.cpp`) SHALL reside in `external/WickedEngine/Editor/` and SHALL be listed only in the Editor CMake target, not in any client library or executable target.
2. THE `DMOAssetPlacerWindow` source files (`DMOAssetPlacerWindow.h`, `DMOAssetPlacerWindow.cpp`) SHALL reside in `external/WickedEngine/Editor/` and SHALL be listed only in the Editor CMake target.
3. THE Editor CMake target SHALL add the DMO client's `Source/Travel/` directory to its include path so that `DMOTravelMapWindow.cpp` can include `WickedTravelMapEditorTool.h` via a relative include without modifying the client's CMakeLists.txt.
4. IF the Editor CMake target is not being built, THEN the `WickedTravelMapEditorTool` and `WickedZoneMapLayoutLoader` translation units SHALL NOT be compiled as part of any other target solely due to the editor extension.
