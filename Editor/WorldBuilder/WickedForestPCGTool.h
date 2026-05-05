// ===============================================
// WICKED-WORLDBUILD-05: Advanced PCG Forest System
// ===============================================
#pragma once

#include "WickedEngine.h"
#include <vector>
#include <string>

class EditorComponent;

// ------------------------------------------------------------------
// Biome Forest Rule (registered in EditorDataAssetRegistry)
// ------------------------------------------------------------------
struct BiomeForestRule
{
    std::string biomeName;           // e.g. "TemperateForest"
    std::vector<std::string> treeMeshAssets;
    std::vector<std::string> undergrowthAssets;
    float       treeDensityPerSqMeter = 0.12f;
    float       undergrowthDensity = 0.45f;
    float       minTreeScale = 0.7f;
    float       maxTreeScale = 1.6f;
    float       maxSlopeAngle = 38.0f;
    float       minHeight = 0.0f;
    float       maxHeight = 9999.0f;
};

// ------------------------------------------------------------------
// PCGForestComponent (runtime forest instance)
// ------------------------------------------------------------------
class PCGForestComponent
{
public:
    static constexpr uint32_t TYPE = 0xBBCCDD22;

    std::vector<wi::ecs::Entity> spawnedTrees;
    std::vector<wi::ecs::Entity> spawnedUndergrowth;
    BiomeForestRule              activeRule;

    void GenerateInArea(const XMFLOAT3& center, float radius);
    void BakeToDataLayer();
};

// ------------------------------------------------------------------
// WickedForestPCGWindow (editor tool)
// ------------------------------------------------------------------
class WickedForestPCGWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

    // UI elements
    wi::gui::ComboBox biomeCombo;
    wi::gui::Slider   radiusSlider;
    wi::gui::CheckBox autoBakeCheck;
    wi::gui::Button   generateButton;

    // Main generation
    void GenerateForestOnCurrentSelection();

private:
    std::vector<BiomeForestRule> m_availableBiomes;
};
