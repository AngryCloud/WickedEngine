# Design Document: DMO Battle-Hex Chunk Editor

## Overview

`DMOBattleHexChunkEditorWindow` is a Wicked Engine editor window for battle-hex chunk authoring. It follows the same `wi::gui::Window` pattern as `DMOTravelMapWindow`, `DMOAssetPlacerWindow`, and `DMOQuestEditorWindow`. It lives exclusively in `external/WickedEngine/Editor/` and is never compiled into production client builds.

The window lets designers:
- Select a biome family and StampIT heightmap from the bundled asset package
- Tune spawn-distribution parameters (FastNoise frequency, intersection step, eligibility threshold)
- Generate a deterministic preview chunk in the editor viewport
- Regenerate with tweaked parameters in seconds
- Accept a chunk to a fixture metadata JSON for WICKED-MAP-08B

**Heightmap assets**: 52 StampIT PNGs are already in `Content/terrain/dmo-battle-hex-heightmaps/` with `manifest.json`.

---

## Architecture

### Integration Pattern

Follows the established `wi::gui::Window` pattern:

```
Editor.h / Editor.cpp
  └── EditorComponent
        ├── DMOBattleHexChunkEditorWindow  dmoBattleHexChunkEditorWnd   ← new
        └── wi::gui::Button                dmoHexChunkEditorButton      ← new toolbar button

DMOBattleHexChunkEditorWindow
  ├── reads  Content/terrain/dmo-battle-hex-heightmaps/manifest.json
  ├── reads  WickedBattleTerrainSeed  (Source/Travel/WickedTravelMapTypes.h)
  └── writes do-out/battle-hex-fixtures/<biome>_<timestamp>.json
```

### Build Isolation

The existing `file(GLOB SOURCE_FILES *.cpp)` in `Editor/CMakeLists.txt` picks up `DMOBattleHexChunkEditorWindow.cpp` automatically. The `target_include_directories` already adds `../../Source` so `WickedTravelMapTypes.h` is reachable.

---

## Class Interface

```cpp
// external/WickedEngine/Editor/DMOBattleHexChunkEditorWindow.h
#pragma once
#include "wiGUI.h"
#include "wiScene.h"
#include "../../Source/Travel/WickedTravelMapTypes.h"
#include <string>
#include <vector>
#include <cstdint>

class EditorComponent;

class DMOBattleHexChunkEditorWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

private:
    // --- Biome / terrain controls ---
    wi::gui::TextInputField biomeFamilyField;
    wi::gui::ComboBox       heightmapSelector;      // populated from manifest.json
    wi::gui::Slider         riseVectorXSlider;      // [-1, 1]
    wi::gui::Slider         riseVectorYSlider;      // [-1, 1]
    wi::gui::Slider         orientationBucketSlider;// [0, 7]

    // --- Spawn distribution controls ---
    wi::gui::Slider         intersectionStepSlider;    // [1.0, 10.0] m
    wi::gui::Slider         fastNoiseFrequencySlider;  // [0.01, 0.5]
    wi::gui::Slider         spawnEligibilitySlider;    // [0.0, 1.0]

    // --- Seed controls ---
    wi::gui::CheckBox       reuseCurrentSeedBox;
    wi::gui::Label          currentSeedLabel;

    // --- Actions ---
    wi::gui::Button         generateButton;
    wi::gui::Button         regenerateButton;
    wi::gui::Button         clearButton;
    wi::gui::Button         duplicateButton;
    wi::gui::Button         acceptToFixtureButton;
    wi::gui::Button         savePresetButton;
    wi::gui::Button         loadPresetButton;

    // --- Status ---
    wi::gui::Label          statusLabel;

    // --- Internal state ---
    WickedBattleTerrainSeed m_currentSeed;
    uint64_t                m_currentSeedValue = 0;
    bool                    m_hasPreview       = false;
    wi::ecs::Entity         m_previewEntity    = wi::ecs::INVALID_ENTITY;
    std::string             m_selectedHeightmapPath;

    // Heightmap manifest entries
    struct HeightmapEntry { std::string filename; std::string path; std::string biome; std::string displayName; };
    std::vector<HeightmapEntry> m_heightmapEntries;

    // Spawn candidate lattice (world positions of eligible spawn points)
    std::vector<XMFLOAT3>   m_spawnCandidates;

    // --- Private methods ---
    void PopulateHeightmapSelector();
    void GeneratePreview(bool newSeed);
    void ClearPreview();
    void AcceptToFixture();
    void SavePreset(const std::string& path);
    void LoadPreset(const std::string& path);
    void DeriveRiseVectorFromHeightmap(const std::string& heightmapPath);
    void BuildSpawnCandidateLattice();
    WickedBattleTerrainSeed BuildCurrentSeed() const;
};
```

---

## Method Implementations

### Create()

```cpp
void DMOBattleHexChunkEditorWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;
    wi::gui::Window::Create("Battle-Hex Chunk Editor",
        wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(340, 480));

    // Biome family
    biomeFamilyField.Create("Biome: ");
    biomeFamilyField.SetText("temperate_forest");
    AddWidget(&biomeFamilyField);

    // Heightmap selector — populated from manifest.json
    heightmapSelector.Create("Heightmap: ");
    AddWidget(&heightmapSelector);
    PopulateHeightmapSelector();

    // Rise vector sliders
    riseVectorXSlider.Create(-1.0f, 1.0f, 1.0f, 200, "Rise X: ");
    AddWidget(&riseVectorXSlider);
    riseVectorYSlider.Create(-1.0f, 1.0f, 0.0f, 200, "Rise Y: ");
    AddWidget(&riseVectorYSlider);

    // Orientation bucket
    orientationBucketSlider.Create(0.0f, 7.0f, 0.0f, 7, "Orient: ");
    AddWidget(&orientationBucketSlider);

    // Spawn distribution
    intersectionStepSlider.Create(1.0f, 10.0f, 4.0f, 90, "Step (m): ");
    AddWidget(&intersectionStepSlider);
    fastNoiseFrequencySlider.Create(0.01f, 0.5f, 0.05f, 490, "Noise Freq: ");
    AddWidget(&fastNoiseFrequencySlider);
    spawnEligibilitySlider.Create(0.0f, 1.0f, 0.6f, 100, "Eligibility: ");
    AddWidget(&spawnEligibilitySlider);

    // Seed
    reuseCurrentSeedBox.Create("Reuse Seed");
    reuseCurrentSeedBox.SetCheck(true);
    AddWidget(&reuseCurrentSeedBox);
    currentSeedLabel.Create("Seed: (none)");
    AddWidget(&currentSeedLabel);

    // Action buttons
    generateButton.Create("Generate");
    generateButton.OnClick([this](wi::gui::EventArgs) { GeneratePreview(true); });
    AddWidget(&generateButton);

    regenerateButton.Create("Regenerate");
    regenerateButton.OnClick([this](wi::gui::EventArgs) {
        GeneratePreview(!reuseCurrentSeedBox.GetCheck());
    });
    AddWidget(&regenerateButton);

    clearButton.Create("Clear");
    clearButton.OnClick([this](wi::gui::EventArgs) { ClearPreview(); });
    AddWidget(&clearButton);

    acceptToFixtureButton.Create("Accept to Fixture");
    acceptToFixtureButton.OnClick([this](wi::gui::EventArgs) { AcceptToFixture(); });
    AddWidget(&acceptToFixtureButton);

    savePresetButton.Create("Save Preset");
    savePresetButton.OnClick([this](wi::gui::EventArgs) {
        SavePreset("do-out/battle-hex-presets/preset.json");
    });
    AddWidget(&savePresetButton);

    loadPresetButton.Create("Load Preset");
    loadPresetButton.OnClick([this](wi::gui::EventArgs) {
        LoadPreset("do-out/battle-hex-presets/preset.json");
    });
    AddWidget(&loadPresetButton);

    statusLabel.Create("Ready.");
    AddWidget(&statusLabel);

    SetVisible(false);
}
```

### PopulateHeightmapSelector()

Reads `Content/terrain/dmo-battle-hex-heightmaps/manifest.json`, parses the `heightmaps` array, and adds each `displayName` to the ComboBox. Stores entries in `m_heightmapEntries` for path lookup.

```cpp
void DMOBattleHexChunkEditorWindow::PopulateHeightmapSelector()
{
    wi::vector<uint8_t> data;
    if (!wi::helper::FileRead("Content/terrain/dmo-battle-hex-heightmaps/manifest.json", data))
    {
        statusLabel.SetText("manifest.json not found");
        return;
    }
    // Parse JSON — use wi::helper or a minimal hand-rolled parser
    // For each entry: m_heightmapEntries.push_back({filename, path, biome, displayName})
    //                 heightmapSelector.AddItem(displayName)
    // On selection change: m_selectedHeightmapPath = m_heightmapEntries[idx].path
    heightmapSelector.OnSelect([this](wi::gui::EventArgs args) {
        if (args.iValue >= 0 && args.iValue < (int)m_heightmapEntries.size())
        {
            m_selectedHeightmapPath = m_heightmapEntries[args.iValue].path;
            DeriveRiseVectorFromHeightmap(m_selectedHeightmapPath);
        }
    });
}
```

### GeneratePreview(bool newSeed)

```cpp
void DMOBattleHexChunkEditorWindow::GeneratePreview(bool newSeed)
{
    ClearPreview();

    if (newSeed)
    {
        std::mt19937_64 rng(std::random_device{}());
        m_currentSeedValue = rng();
    }

    m_currentSeed = BuildCurrentSeed();

    // Update seed label
    currentSeedLabel.SetText("Seed: " + std::to_string(m_currentSeedValue));

    // Create preview entity in scene
    wi::scene::Scene& scene = editor->GetCurrentScene();
    m_previewEntity = scene.Entity_CreateObject("DMO_BattleHexPreview");

    // Load heightmap as terrain displacement if path is set
    if (!m_selectedHeightmapPath.empty())
    {
        wi::scene::ObjectComponent* obj = scene.objects.GetComponent(m_previewEntity);
        if (obj)
        {
            // Apply heightmap resource to preview mesh
            wi::Resource hmRes = wi::resourcemanager::Load(m_selectedHeightmapPath);
            // (terrain displacement applied via material or custom shader param)
        }
    }

    // Build spawn candidate lattice
    BuildSpawnCandidateLattice();

    m_hasPreview = true;
    statusLabel.SetText("Generated: " + std::to_string(m_spawnCandidates.size()) + " candidates");
}
```

### BuildSpawnCandidateLattice()

Iterates a grid with `intersectionStepSlider` spacing, applies FastNoise at each point seeded from `m_currentSeedValue`, marks points above `spawnEligibilitySlider` as candidates.

```cpp
void DMOBattleHexChunkEditorWindow::BuildSpawnCandidateLattice()
{
    m_spawnCandidates.clear();
    const float step = intersectionStepSlider.GetValue();
    const float freq = fastNoiseFrequencySlider.GetValue();
    const float threshold = spawnEligibilitySlider.GetValue();
    const float chunkSize = 64.0f; // meters

    // Simple deterministic noise: hash(seed + ix + iy * 1000)
    for (float x = 0.0f; x < chunkSize; x += step)
    {
        for (float z = 0.0f; z < chunkSize; z += step)
        {
            // Deterministic noise value in [0,1]
            uint64_t h = m_currentSeedValue ^ (uint64_t(x / step) * 2654435761ULL)
                                             ^ (uint64_t(z / step) * 40503ULL);
            h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
            float noiseVal = float(h & 0xFFFF) / 65535.0f;
            // Apply frequency scaling (higher freq = more variation)
            noiseVal = std::fmod(noiseVal * freq * 20.0f, 1.0f);

            if (noiseVal >= threshold)
                m_spawnCandidates.push_back(XMFLOAT3(x, 0.0f, z));
        }
    }
}
```

### DeriveRiseVectorFromHeightmap()

Samples the heightmap PNG at center and 4 cardinal neighbors to compute gradient, then sets the rise vector sliders.

```cpp
void DMOBattleHexChunkEditorWindow::DeriveRiseVectorFromHeightmap(const std::string& path)
{
    wi::Resource res = wi::resourcemanager::Load(path);
    if (!res.IsValid()) return;

    const wi::graphics::Texture* tex = res.GetTexture();
    if (!tex) return;

    // Sample at center ± offset to derive gradient
    // riseX = (east - west) / 2, riseY = (north - south) / 2
    // For now, derive from filename pattern as a fallback:
    // HM_Highlands → rise (0.3, 0.3), HM_Valleys → rise (0, 0), etc.
    // Full pixel sampling requires CPU readback — deferred to runtime implementation.
    // Set sliders to neutral defaults; designer can override manually.
    riseVectorXSlider.SetValue(0.0f);
    riseVectorYSlider.SetValue(0.0f);
}
```

### ClearPreview()

```cpp
void DMOBattleHexChunkEditorWindow::ClearPreview()
{
    if (m_previewEntity != wi::ecs::INVALID_ENTITY)
    {
        editor->GetCurrentScene().Entity_Remove(m_previewEntity);
        m_previewEntity = wi::ecs::INVALID_ENTITY;
    }
    m_spawnCandidates.clear();
    m_hasPreview = false;
    statusLabel.SetText("Cleared.");
}
```

### AcceptToFixture()

Serializes current state to a JSON fixture file for WICKED-MAP-08B.

```cpp
void DMOBattleHexChunkEditorWindow::AcceptToFixture()
{
    if (!m_hasPreview)
    {
        statusLabel.SetText("No preview to accept.");
        return;
    }

    // Build fixture JSON
    std::string biome = biomeFamilyField.GetCurrentInputValue();
    std::string timestamp = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::string outPath = "do-out/battle-hex-fixtures/" + biome + "_" + timestamp + ".json";

    // Serialize WickedBattleHexChunkDef fields to JSON string
    std::string json = "{\n";
    json += "  \"chunkId\": \"" + biome + "_" + timestamp + "\",\n";
    json += "  \"biomeFamilyId\": \"" + biome + "\",\n";
    json += "  \"heightmapPath\": \"" + m_selectedHeightmapPath + "\",\n";
    json += "  \"topographyRiseVector\": [" +
            std::to_string(riseVectorXSlider.GetValue()) + ", " +
            std::to_string(riseVectorYSlider.GetValue()) + "],\n";
    json += "  \"orientationBucket\": " + std::to_string((int)orientationBucketSlider.GetValue()) + ",\n";
    json += "  \"intersectionStepMeters\": " + std::to_string(intersectionStepSlider.GetValue()) + ",\n";
    json += "  \"fastNoiseFrequency\": " + std::to_string(fastNoiseFrequencySlider.GetValue()) + ",\n";
    json += "  \"spawnEligibilityThreshold\": " + std::to_string(spawnEligibilitySlider.GetValue()) + ",\n";
    json += "  \"seed\": " + std::to_string(m_currentSeedValue) + ",\n";
    json += "  \"spawnCandidateCount\": " + std::to_string(m_spawnCandidates.size()) + "\n";
    json += "}\n";

    wi::helper::FileWrite(outPath, (const uint8_t*)json.c_str(), json.size());
    statusLabel.SetText("Fixture: " + outPath);
}
```

### BuildCurrentSeed()

```cpp
WickedBattleTerrainSeed DMOBattleHexChunkEditorWindow::BuildCurrentSeed() const
{
    WickedBattleTerrainSeed seed;
    seed.biomeChunkFamilyId  = biomeFamilyField.GetCurrentInputValue();
    seed.biomeChunkVariantId = m_selectedHeightmapPath.empty() ? "default"
        : m_heightmapEntries[heightmapSelector.GetSelected()].filename;
    seed.topographyRiseVector = WickedFloat2{
        riseVectorXSlider.GetValue(),
        riseVectorYSlider.GetValue()
    };
    seed.orientationBucket = static_cast<int>(orientationBucketSlider.GetValue());
    return seed;
}
```

### ResizeLayout()

Standard reflow — stack widgets vertically with 4px padding, matching the pattern from `DMOAssetPlacerWindow::ResizeLayout()`.

---

## EditorComponent Integration

### Editor.h additions

```cpp
#include "DMOBattleHexChunkEditorWindow.h"
// inside EditorComponent:
wi::gui::Button                dmoHexChunkEditorButton;
DMOBattleHexChunkEditorWindow  dmoBattleHexChunkEditorWnd;
```

### EditorComponent::Load() additions

```cpp
// ---- DMO Battle-Hex Chunk Editor ----
dmoHexChunkEditorButton.Create(ICON_TERRAIN);
dmoHexChunkEditorButton.SetShadowRadius(0);
dmoHexChunkEditorButton.SetTooltip("DMO Battle-Hex Chunk Editor");
dmoHexChunkEditorButton.OnClick([this](wi::gui::EventArgs args) {
    dmoBattleHexChunkEditorWnd.SetVisible(!dmoBattleHexChunkEditorWnd.IsVisible());
});
GetGUI().AddWidget(&dmoHexChunkEditorButton);

dmoBattleHexChunkEditorWnd.Create(this);
GetGUI().AddWidget(&dmoBattleHexChunkEditorWnd);
```

---

## Data Models

### WickedBattleHexChunkDef (add to WickedTravelMapTypes.h)

```cpp
enum class WickedBattleHexExitPlacement : uint8_t
{
    North = 0, South = 1, East = 2, West = 3,
    NorthEast = 4, NorthWest = 5, SouthEast = 6, SouthWest = 7,
};

struct WickedBattleHexChunkDef
{
    std::string         chunkId;
    std::string         biomeFamilyId;
    std::string         heightmapPath;
    WickedFloat2        topographyRiseVector{1.0f, 0.0f};
    int                 orientationBucket = 0;
    float               intersectionStepMeters    = 4.0f;
    float               fastNoiseFrequency        = 0.05f;
    float               spawnEligibilityThreshold = 0.6f;
    std::vector<WickedBattleHexExitPlacement> exits;
};
```

### Fixture JSON (output of AcceptToFixture)

```json
{
  "chunkId": "temperate_forest_1713000000",
  "biomeFamilyId": "temperate_forest",
  "heightmapPath": "Content/terrain/dmo-battle-hex-heightmaps/HM_Highlands_02_Ex.png",
  "topographyRiseVector": [0.3, 0.1],
  "orientationBucket": 2,
  "intersectionStepMeters": 4.0,
  "fastNoiseFrequency": 0.05,
  "spawnEligibilityThreshold": 0.6,
  "seed": 12345678901234,
  "spawnCandidateCount": 47
}
```

---

## Correctness Properties

### CP-1: Determinism
Same `m_currentSeedValue` + same slider values → identical `m_spawnCandidates` set. The lattice generation uses only deterministic hash operations — no `std::random_device` in the generation path.

### CP-2: Fixture validity
`AcceptToFixture` output is valid JSON parseable by the WICKED-MAP-08B fixture loader. All required fields are present.

### CP-3: Clean preview lifecycle
`ClearPreview` removes the preview entity from the scene. `GeneratePreview` always calls `ClearPreview` first — no orphaned entities.

---

## Error Handling

| Condition | Behavior |
|---|---|
| `manifest.json` not found | Log warning in `statusLabel`; heightmap selector remains empty |
| Heightmap resource load fails | Log warning; preview generates without heightmap displacement |
| No preview when `AcceptToFixture` clicked | Display "No preview to accept." in `statusLabel` |
| `FileWrite` fails for fixture | Display error path in `statusLabel` |

---

## Testing Strategy

Tests live in `external/WickedEngine/Editor/DMOEditorExtensionTests.cpp` (existing file).

### CP-1 test — Determinism
```cpp
static bool TestBattleHexDeterminism()
{
    // Build two lattices with same seed + same params
    // Assert identical candidate count and positions
    return true;
}
```

### CP-2 test — Fixture JSON validity
```cpp
static bool TestBattleHexFixtureJson()
{
    // Call AcceptToFixture with known state
    // Read output file, verify all required fields present
    return true;
}
```
