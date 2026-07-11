# Design Document: DMO Editor Extension

## Overview

The DMO Editor Extension adds four authoring tools to the Wicked Engine Editor fork:

1. **DMOTravelMapWindow** — a 2D map editor panel for positioning zone anchors, city pins, and place-name labels on the Oracle travel map texture, backed by `WickedTravelMapEditorTool`.
2. **Play From Here** — a toolbar button + keyboard shortcut (F5) that starts scene playback from a user-defined spawn marker persisted in `wi::config::File`.
3. **DMOAssetPlacerWindow** — a surface-snap/grid/brush/free placement window with ghost preview, randomizers, and undo support.
4. **DMOBattleHexChunkEditorWindow** — a battle-hex chunk authoring panel for generating deterministic spawn-candidate previews from StampIT heightmaps and biome parameters, with fixture export for WICKED-MAP-08B. See `battle-hex-chunk-editor/design.md` for the full design.

All four tools live exclusively in `external/WickedEngine/Editor/` and are compiled only into the `WickedEngineEditor` CMake target. No editor-only code reaches production client builds.

---

## Architecture

### Integration Pattern

All new windows follow the established `wi::gui::Window` pattern used by `PaintToolWindow` and `TerrainWindow`:

- Inherit from `wi::gui::Window`
- Expose `void Create(EditorComponent* editor)`
- Store `EditorComponent* editor = nullptr` as a member
- Override `ResizeLayout()` for responsive layout
- Declared as value members of `EditorComponent` in `Editor.h`
- Initialized in `EditorComponent::Load()` in `Editor.cpp`

### Layered Responsibilities

```
Editor.h / Editor.cpp
  └── EditorComponent
        ├── DMOTravelMapWindow dmoTravelMapWnd              ← implemented
        ├── DMOAssetPlacerWindow dmoAssetPlacerWnd          ← implemented
        ├── DMOBattleHexChunkEditorWindow dmoBattleHexChunkEditorWnd  ← new (WICKED-MAP-08A)
        ├── wi::gui::Button dmoTravelMapButton              ← implemented
        ├── wi::gui::Button dmoAssetPlacerButton            ← implemented
        ├── wi::gui::Button dmoHexChunkEditorButton         ← new
        ├── wi::gui::Button dmoPlayFromHereButton           ← implemented
        └── SpawnMarker state (XMFLOAT3 pos + float yaw/pitch)

DMOTravelMapWindow
  └── WickedTravelMapEditorTool m_tool   ← pure data layer, no GPU
        └── WickedZoneMapLayoutLoader    ← JSON I/O

DMOAssetPlacerWindow
  └── wi::ecs::Entity m_ghostEntity      ← scene entity, 50% opacity

DMOBattleHexChunkEditorWindow
  ├── reads  Content/terrain/dmo-battle-hex-heightmaps/manifest.json
  ├── reads  WickedBattleTerrainSeed  (Source/Travel/WickedTravelMapTypes.h)
  └── writes do-out/battle-hex-fixtures/<biome>_<timestamp>.json
```

### Build Isolation

```cmake
# external/WickedEngine/Editor/CMakeLists.txt — only change needed:
target_include_directories(WickedEngineEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Source)   # DMO client Source/
```

The existing `file(GLOB SOURCE_FILES *.cpp)` in the Editor CMakeLists already picks up all `*.cpp` files in the Editor directory, so `DMOTravelMapWindow.cpp`, `DMOAssetPlacerWindow.cpp`, and `DMOEditorExtensionTests.cpp` are included automatically. Only the include path addition is a manual CMake change.

---

## Components and Interfaces

### DMOTravelMapWindow

**Files:** `Editor/DMOTravelMapWindow.h` + `DMOTravelMapWindow.cpp`

```cpp
class DMOTravelMapWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;

    EditorComponent* editor = nullptr;

private:
    // Held as unique_ptr to avoid pulling client headers into this header.
    // Allocated in Create(); never null after Create() returns.
    std::unique_ptr<WickedTravelMapEditorTool> m_tool;
    bool        m_loaded = false;
    std::string m_layoutPath;

    // Selected anchor
    std::string m_selectedAnchorId;
    bool        m_dragging = false;
    XMFLOAT2    m_dragStartMouse{};
    WickedFloat2 m_dragStartUV{};

    // Filter state (render-only, never passed to tool)
    bool m_showZones   = true;
    bool m_showCities  = true;
    bool m_showLabels  = true;

    // Canvas geometry (computed in ResizeLayout)
    XMFLOAT2 m_canvasOrigin{};
    XMFLOAT2 m_canvasSize{};

    // UI widgets
    wi::gui::Button     loadButton;
    wi::gui::Button     saveButton;
    wi::gui::Button     importLabelsButton;
    wi::gui::Label      statusLabel;
    wi::gui::CheckBox   zonesCheckBox;
    wi::gui::CheckBox   citiesCheckBox;
    wi::gui::CheckBox   labelsCheckBox;

    // Properties panel (shown when an anchor is selected)
    wi::gui::Label          propIdLabel;
    wi::gui::Label          propKindLabel;
    wi::gui::Label          propUVLabel;
    wi::gui::TextInputField propNameField;   // PlaceName only
    wi::gui::CheckBox       propVisibleBox;  // PlaceName only
    wi::gui::Slider         propConfSlider;  // PlaceName only

    // Map texture resource (lazy-loaded on first Render call)
    mutable wi::Resource m_mapTexture;
    mutable bool         m_mapTextureLoaded = false;

    // Hit test priority: Labels (PlaceName) → Cities (InstancedCity) → Zones
    XMFLOAT2     UVToCanvas(float u, float v) const;
    void         CanvasToUV(const XMFLOAT2& pos, float& outU, float& outV) const;
    std::string  HitTest(const XMFLOAT2& clickPos) const;
    void         RefreshPropertiesPanel();
    void         ClearPropertiesPanel();
    void         SetStatus(const std::string& msg);
};
```

**UV ↔ Canvas conversion:**
```
canvasPos = canvasOrigin + uv * canvasSize
uv        = clamp((clickPos - canvasOrigin) / canvasSize, 0, 1)
```

**Aspect-ratio-preserving scale** (used in `ResizeLayout` and `Render`):
```
sourceAspect = 2163.0f / 3336.0f
if (canvasSize.x / canvasSize.y > sourceAspect)
    drawW = canvasSize.y * sourceAspect,  drawH = canvasSize.y
else
    drawW = canvasSize.x,                 drawH = canvasSize.x / sourceAspect
```

**Anchor overlay rendering:**
- Zone boxes: `wi::image::Draw` with `params.siz = sizePx * canvasScale`, `params.pos = UVToCanvas(centerUV) - siz/2`
- City pins: `wi::image::Draw` with 8×8 white texture at `UVToCanvas(centerUV)`
- Labels: `wi::font::Draw(displayName, fontParams, cmd)` at `UVToCanvas(centerUV)`, font size 10
- Selected anchor: highlight color `XMFLOAT4(1.0f, 0.85f, 0.0f, 1.0f)`
- Hidden labels: `fontParams.color.w = 0.4f`

**Hit test priority:** Labels (PlaceName) → Cities (InstancedCity) → Zones. First match in priority order wins.

---

### Play From Here

**Files:** Inline in `Editor.h` / `Editor.cpp` — no separate window class needed.

**State added to `EditorComponent`:**
```cpp
// Play From Here
wi::gui::Button dmoPlayFromHereButton;
bool            dmoSpawnMarkerSet = false;
XMFLOAT3        dmoSpawnPos{};
float           dmoSpawnYaw   = 0.0f;
float           dmoSpawnPitch = 0.0f;
```

**Config keys:** `dmo.spawn.x`, `dmo.spawn.y`, `dmo.spawn.z`, `dmo.spawn.yaw`, `dmo.spawn.pitch`

**Spawn marker placement:** On right-click in 3D viewport while Play From Here mode is active, cast a pick ray via `wi::scene::Pick`. If a valid hit is found, store `hit.position` as `dmoSpawnPos` and derive yaw/pitch from the camera's current orientation. Write all five keys to `main->config` and call `main->config.Commit()`.

**Playback activation (button click or F5):**
1. If `!dmoSpawnMarkerSet`: post status message "No spawn point set. Right-click in viewport to place one." and return.
2. Otherwise: set the player start transform in the scene to `dmoSpawnPos` + yaw/pitch, then invoke the existing `playButton` click logic.

**F6 stop:** Invoke the existing `stopButton` click logic.

**Config persistence:** On `EditorComponent::Load()`, read the five keys from `main->config` if present and restore `dmoSpawnPos`, `dmoSpawnYaw`, `dmoSpawnPitch`, `dmoSpawnMarkerSet = true`.

---

### DMOAssetPlacerWindow

**Files:** `Editor/DMOAssetPlacerWindow.h` + `DMOAssetPlacerWindow.cpp`

```cpp
class DMOAssetPlacerWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

private:
    enum class PlacementMode { Free, Grid, SurfaceSnap, Brush };
    PlacementMode m_mode = PlacementMode::Free;

    wi::ecs::Entity m_ghostEntity = wi::ecs::INVALID_ENTITY;
    std::string     m_selectedAssetPath;

    // Per-stroke accumulator for brush undo
    wi::vector<wi::ecs::Entity> m_strokeEntities;

    // Scroll-wheel overrides (applied on top of randomizers)
    float m_scrollYRotation = 0.0f;
    float m_scrollHeightOfs = 0.0f;
    float m_scrollScale     = 1.0f;

    // UI widgets
    wi::gui::ComboBox modeCombo;
    wi::gui::Slider   gridSizeSlider;       // Grid mode
    wi::gui::Slider   brushRadiusSlider;    // Brush mode
    wi::gui::Slider   brushDensitySlider;   // Brush mode
    wi::gui::Slider   scaleMinSlider;
    wi::gui::Slider   scaleMaxSlider;
    wi::gui::Slider   rotYRangeSlider;
    wi::gui::Slider   tiltRangeSlider;

    // Helpers
    void     UpdateGhostEntity(const XMFLOAT3& worldPos, const XMFLOAT3& normal);
    void     HideGhostEntity();
    void     RemoveGhostEntity();
    XMFLOAT3 SnapToGrid(const XMFLOAT3& worldPos, float gridSize) const;
    int      ComputeBrushCount(float density, float radius) const;
    XMFLOAT3 ApplyRandomizers(const XMFLOAT3& basePos, const XMFLOAT3& normal) const;
    void     PlaceInstance(const XMFLOAT3& pos, const XMFLOAT3& normal, bool recordHistory);
    void     CommitBrushStroke();
};
```

**Grid snapping:**
```
snapped.x = round(worldPos.x / gridSize) * gridSize
snapped.z = round(worldPos.z / gridSize) * gridSize
snapped.y = worldPos.y  (unchanged; surface-snap handles Y separately)
```

**Brush count formula:**
```
maxCount = floor(density * radius * radius * π)
count    = clamp(maxCount, 1, maxCount)
```

**Ghost entity lifecycle:**
- Created via `wi::scene::Entity_CreateObject(scene, "DMO_GhostPreview")` on first use
- `ObjectComponent::color.w = 0.5f` for 50% opacity
- Hidden (moved off-screen or disabled) when cursor leaves viewport or no asset selected
- Removed from scene on window close or mode change to Brush

**Scroll wheel handling (in `Update`):**
- No modifier: `m_scrollYRotation += ticks * 15.0f`
- Shift held: `m_scrollHeightOfs += ticks * 0.1f`
- Ctrl held: `m_scrollScale *= pow(1.1f, ticks)`, clamped to [0.01, 10.0]

**Undo integration:**
- Single placement: `editor->AdvanceHistory()` records the entity
- Brush stroke: accumulate entities in `m_strokeEntities`; on mouse release call `editor->AdvanceHistory()` once with all entities
- `ConsumeHistoryOperation` removes the recorded entities from the scene

---

## Data Models

### SpawnMarker (in-memory, EditorComponent)

| Field | Type | Config Key |
|---|---|---|
| `dmoSpawnPos.x` | float | `dmo.spawn.x` |
| `dmoSpawnPos.y` | float | `dmo.spawn.y` |
| `dmoSpawnPos.z` | float | `dmo.spawn.z` |
| `dmoSpawnYaw` | float (degrees) | `dmo.spawn.yaw` |
| `dmoSpawnPitch` | float (degrees) | `dmo.spawn.pitch` |

### WickedZoneMapLayout (owned by WickedTravelMapEditorTool)

Already defined in `WickedTravelMapTypes.h`. The editor window holds a `WickedTravelMapEditorTool` by value; the tool owns the layout. The window never accesses the layout directly — only through the tool's public API.

### Filter State (DMOTravelMapWindow)

Three `bool` members (`m_showZones`, `m_showCities`, `m_showLabels`) control render-time filtering. They are never written to disk and never passed to `WickedTravelMapEditorTool`.

### Asset Placer State (DMOAssetPlacerWindow)

Transient per-session state only. No persistence. The ghost entity is a live scene entity; its transform is the source of truth for placement position.

---

## Correctness Properties

### Property 1: Drag round-trip preserves centerUV

*For any* anchor in a loaded layout, dragging it to a new canvas position and then performing SaveLayout → LoadLayout SHALL produce a `centerUV` equal to the value passed to `SetAnchorCenterUV`, within floating-point round-trip precision (absolute error ≤ 1e-5 per component).

**Validates: Requirements 6.1, 6.3, 9.1, 9.4**

### Property 2: Filter toggles do not mutate anchor data

*For any* loaded layout and any sequence of filter checkbox state changes, the data returned by `WickedTravelMapEditorTool::GetAnchors()` SHALL be identical before and after the filter operations — no mutation method SHALL be called as a side effect of filter toggling.

**Validates: Requirements 4.2, 4.3**

### Property 3: ImportPlaceNames round-trip preserves label count

*For any* valid OCR JSON file, the sequence ImportPlaceNames → SaveLayout → LoadLayout SHALL produce a layout whose `PlaceName` anchor count equals the count returned by `ImportPlaceNames`.

**Validates: Requirements 8.2, 9.1, 9.4**

### Property 4: SpawnMarker round-trips through wi::config::File

*For any* spawn position `(x, y, z)` and orientation `(yaw, pitch)` written to `wi::config::File`, reading those keys back SHALL produce values within absolute error ≤ 1e-4 per component.

**Validates: Requirements 10.3, 12.1, 12.2**

### Property 5: Brush stroke instance count is within bounds

*For any* brush density `d ∈ [0.1, 10.0]` and radius `r ∈ [0.5, 50.0]`, the number of instances placed during a single brush stroke SHALL satisfy `1 ≤ count ≤ floor(d × r² × π)`.

**Validates: Requirement 16.2**

---

## Error Handling

### DMOTravelMapWindow

| Condition | Behavior |
|---|---|
| `LoadLayout` returns false | Display "Load failed." in status label; leave MapCanvas unchanged |
| `SaveLayout` returns false | Display "Save failed." in status label; leave in-memory layout unchanged |
| `ImportPlaceNames` returns 0 | Display "Import failed: 0 labels imported"; do not refresh MapCanvas |
| Click outside MapCanvas bounds | Ignore; do not deselect current anchor |
| Drag outside MapCanvas bounds | Clamp UV to [0,1]×[0,1] before calling `SetAnchorCenterUV` |
| No layout loaded when Save clicked | Display "No layout loaded." in status label |

### Play From Here

| Condition | Behavior |
|---|---|
| Button clicked with no spawn marker | Display "No spawn point set. Right-click in viewport to place one." |
| Right-click pick ray misses all geometry | Do not update spawn marker |
| Config keys absent on startup | Leave `dmoSpawnMarkerSet = false`; no error |

### DMOAssetPlacerWindow

| Condition | Behavior |
|---|---|
| No asset selected when clicking | No placement; ghost entity hidden |
| Pick ray misses geometry in Surface-snap mode | Hide ghost entity; no placement on click |
| Scale Min > Scale Max | Clamp Scale Min to Scale Max before applying randomizer |
| Ghost entity creation fails | Log warning; continue without ghost preview |
| Brush stroke produces 0 valid picks | Record no history entry; no-op |

---

## Testing Strategy

### What is and is not testable

The editor windows require a running GPU context and cannot be unit tested in isolation. The five correctness properties are tested at the data-layer and math-layer, which have no GPU dependency.

### Standalone test file

`external/WickedEngine/Editor/DMOEditorExtensionTests.cpp` — compiled as part of the Editor target. Uses no GPU resources.

### Property 5 test — Brush density bounds (pure math, no GPU)

```cpp
// Feature: dmo-editor-extension, Property 5: Brush stroke instance count is within bounds
static bool TestBrushDensityBounds()
{
    // Spot-check a range of density/radius combinations
    const std::vector<std::pair<float,float>> cases = {
        {0.1f, 0.5f}, {1.0f, 5.0f}, {5.0f, 20.0f}, {10.0f, 50.0f}
    };
    for (auto [d, r] : cases)
    {
        const int maxCount = static_cast<int>(std::floor(d * r * r * 3.14159265f));
        const int count = ComputeBrushCount(d, r);
        assert(count >= 1);
        assert(count <= std::max(1, maxCount));
    }
    return true;
}
```

### Property 4 test — SpawnMarker config round-trip (no GPU)

```cpp
// Feature: dmo-editor-extension, Property 4: SpawnMarker round-trips through wi::config::File
static bool TestSpawnMarkerRoundTrip()
{
    wi::config::File cfg;
    const float x = 123.456f, y = -7.89f, z = 0.001f;
    const float yaw = 45.0f, pitch = -10.0f;
    cfg.Set("dmo.spawn.x", x);
    cfg.Set("dmo.spawn.y", y);
    cfg.Set("dmo.spawn.z", z);
    cfg.Set("dmo.spawn.yaw", yaw);
    cfg.Set("dmo.spawn.pitch", pitch);
    assert(std::fabs(cfg.GetFloat("dmo.spawn.x", 0.0f) - x) < 1e-4f);
    assert(std::fabs(cfg.GetFloat("dmo.spawn.y", 0.0f) - y) < 1e-4f);
    assert(std::fabs(cfg.GetFloat("dmo.spawn.z", 0.0f) - z) < 1e-4f);
    assert(std::fabs(cfg.GetFloat("dmo.spawn.yaw", 0.0f) - yaw) < 1e-4f);
    assert(std::fabs(cfg.GetFloat("dmo.spawn.pitch", 0.0f) - pitch) < 1e-4f);
    return true;
}
```

Properties 1, 2, and 3 are substantially covered by `TravelMapAssetMigrationSmoke.cpp` in the client codebase. The editor tests focus on the UV clamping path and the filter immutability invariant.

### Integration tests (manual, require editor runtime)

- Load layout → drag anchor → save → reload → verify position (P1 end-to-end)
- Toggle filter checkboxes → verify no mutation (P2 end-to-end)
- Import labels → save → reload → verify count (P3 end-to-end)
- Build isolation: Editor target compiles with DMO source files; client library does not
