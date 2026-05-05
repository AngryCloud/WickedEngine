// Editor/DMOProceduralTexturingWindow.cpp
// DMO Procedural Texturing editor panel — WICKED-PROC-TEX subsystem tools.

#include "stdafx.h"
#include "DMOProceduralTexturingWindow.h"
#include "Editor.h"

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

void DMOProceduralTexturingWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Procedural Texturing", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(340, 620));

    // ================================================================
    // Template Inspector
    // ================================================================
    templateInspectorLabel.Create("--- Template Inspector ---");
    templateInspectorLabel.SetColor(wi::Color(200, 200, 255, 255));
    AddWidget(&templateInspectorLabel);

    templateTypeCombo.Create("Template: ");
    templateTypeCombo.AddItem("DungeonWall");
    templateTypeCombo.AddItem("ForestFloor");
    templateTypeCombo.AddItem("MetalArmor");
    templateTypeCombo.AddItem("CreatureLeather");
    templateTypeCombo.SetSelected(0);
    templateTypeCombo.SetTooltip("Select a procedural material template to inspect");
    templateTypeCombo.OnSelect([this](wi::gui::EventArgs args) {
        RefreshTemplateInspector();
    });
    AddWidget(&templateTypeCombo);

    templateNameLabel.Create("Name: DungeonWall");
    AddWidget(&templateNameLabel);

    templateLayerCountLabel.Create("Layers: 2");
    AddWidget(&templateLayerCountLabel);

    templateNeuralLabel.Create("Neural augmentation: No");
    AddWidget(&templateNeuralLabel);

    templateLayersDetailLabel.Create("Rust (18.0 Hz), Moss (12.0 Hz)");
    templateLayersDetailLabel.SetTooltip("Pattern types and frequencies in this template");
    AddWidget(&templateLayersDetailLabel);

    // ================================================================
    // Rule Painter
    // ================================================================
    rulePainterLabel.Create("--- Rule Painter ---");
    rulePainterLabel.SetColor(wi::Color(200, 255, 200, 255));
    AddWidget(&rulePainterLabel);

    ruleLayerCombo.Create("Data Layer: ");
    ruleLayerCombo.AddItem("biome_forest");
    ruleLayerCombo.AddItem("battle_damaged");
    ruleLayerCombo.AddItem("fatigue_high");
    ruleLayerCombo.AddItem("dirt_base");
    ruleLayerCombo.AddItem("leaf_litter");
    ruleLayerCombo.AddItem("creature_leather");
    ruleLayerCombo.AddItem("creature_armor");
    ruleLayerCombo.SetSelected(0);
    ruleLayerCombo.SetTooltip("Data layer rule to paint onto selected entities");
    ruleLayerCombo.OnSelect([this](wi::gui::EventArgs args) {
        RefreshRuleInfo();
    });
    AddWidget(&ruleLayerCombo);

    rulePatternCountLabel.Create("Patterns in rule: 2");
    AddWidget(&rulePatternCountLabel);

    paintRuleButton.Create("Paint Rule on Selection");
    paintRuleButton.SetTooltip("Apply the selected data layer rule to the currently selected entity");
    paintRuleButton.OnClick([this](wi::gui::EventArgs args) {
        // Future: paint rule via WickedProceduralTexturing::GetRuleSystem().PaintRuleOnEntity()
    });
    AddWidget(&paintRuleButton);

    clearEntityRulesButton.Create("Clear Entity Rules");
    clearEntityRulesButton.SetTooltip("Remove all procedural rules from the selected entity");
    clearEntityRulesButton.OnClick([this](wi::gui::EventArgs args) {
        // Future: clear rules via WickedProceduralTexturing::GetRuleSystem().ClearEntityRules()
    });
    AddWidget(&clearEntityRulesButton);

    paintedCountLabel.Create("Painted entities: 0");
    AddWidget(&paintedCountLabel);

    // ================================================================
    // Quality Controls
    // ================================================================
    qualityLabel.Create("--- Quality Controls ---");
    qualityLabel.SetColor(wi::Color(255, 255, 200, 255));
    AddWidget(&qualityLabel);

    qualityEnableCheck.Create("Procedural Texturing Enabled");
    qualityEnableCheck.SetCheck(true);
    qualityEnableCheck.SetTooltip("Master enable/disable for the procedural texturing subsystem");
    qualityEnableCheck.OnClick([this](wi::gui::EventArgs args) {
        RefreshQualityInfo();
    });
    AddWidget(&qualityEnableCheck);

    qualityPresetCombo.Create("Quality: ");
    qualityPresetCombo.AddItem("Low");
    qualityPresetCombo.AddItem("Medium");
    qualityPresetCombo.AddItem("High");
    qualityPresetCombo.SetSelected(1); // Medium default
    qualityPresetCombo.SetTooltip("Quality preset — controls pattern density, frequency scaling, and template gating");
    qualityPresetCombo.OnSelect([this](wi::gui::EventArgs args) {
        RefreshQualityInfo();
    });
    AddWidget(&qualityPresetCombo);

    qualityDensityLabel.Create("Density scale: 0.75");
    AddWidget(&qualityDensityLabel);

    qualityFrequencyLabel.Create("Frequency scale: 0.90");
    AddWidget(&qualityFrequencyLabel);

    qualityMaxLayersLabel.Create("Max layers/template: 2");
    AddWidget(&qualityMaxLayersLabel);

    // ================================================================
    // Bake Tools
    // ================================================================
    bakeLabel.Create("--- Bake Tools ---");
    bakeLabel.SetColor(wi::Color(255, 200, 200, 255));
    AddWidget(&bakeLabel);

    bakeTemplateCombo.Create("Bake Target: ");
    bakeTemplateCombo.AddItem("DungeonWall");
    bakeTemplateCombo.AddItem("ForestFloor");
    bakeTemplateCombo.AddItem("MetalArmor");
    bakeTemplateCombo.AddItem("CreatureLeather");
    bakeTemplateCombo.SetSelected(0);
    bakeTemplateCombo.SetTooltip("Material template to bake into a precomputed texture");
    AddWidget(&bakeTemplateCombo);

    bakeButton.Create("Bake Selected Template");
    bakeButton.SetTooltip("Bake the selected template's procedural layers into a lookup texture");
    bakeButton.OnClick([this](wi::gui::EventArgs args) {
        int sel = bakeTemplateCombo.GetSelected();
        std::string name = (sel >= 0) ? bakeTemplateCombo.GetItemText(sel) : "Unknown";
        bakeStatusLabel.SetText("Baking " + name + "...");
        // Future: dispatch offline bake pass
    });
    AddWidget(&bakeButton);

    bakeAllButton.Create("Bake All Templates");
    bakeAllButton.SetTooltip("Bake all four material templates into precomputed lookup textures");
    bakeAllButton.OnClick([this](wi::gui::EventArgs args) {
        bakeStatusLabel.SetText("Baking all templates...");
    });
    AddWidget(&bakeAllButton);

    bakeStatusLabel.Create("Status: Idle");
    AddWidget(&bakeStatusLabel);

    SetVisible(false);

    // Populate initial values
    RefreshTemplateInspector();
    RefreshRuleInfo();
    RefreshQualityInfo();
}

// ---------------------------------------------------------------------------
// ResizeLayout
// ---------------------------------------------------------------------------

void DMOProceduralTexturingWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();

    const float padding  = 4.0f;
    const float itemH    = 18.0f;
    const float labelH   = 16.0f;
    const float winW     = GetSize().x;
    const float contentW = winW - padding * 2.0f;

    float y = padding;

    // Template Inspector
    templateInspectorLabel.SetPos(XMFLOAT2(padding, y));
    templateInspectorLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    templateTypeCombo.SetPos(XMFLOAT2(padding, y));
    templateTypeCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    templateNameLabel.SetPos(XMFLOAT2(padding, y));
    templateNameLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    templateLayerCountLabel.SetPos(XMFLOAT2(padding, y));
    templateLayerCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    templateNeuralLabel.SetPos(XMFLOAT2(padding, y));
    templateNeuralLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    templateLayersDetailLabel.SetPos(XMFLOAT2(padding, y));
    templateLayersDetailLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding * 2.0f;

    // Rule Painter
    rulePainterLabel.SetPos(XMFLOAT2(padding, y));
    rulePainterLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    ruleLayerCombo.SetPos(XMFLOAT2(padding, y));
    ruleLayerCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    rulePatternCountLabel.SetPos(XMFLOAT2(padding, y));
    rulePatternCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    paintRuleButton.SetPos(XMFLOAT2(padding, y));
    paintRuleButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    clearEntityRulesButton.SetPos(XMFLOAT2(padding, y));
    clearEntityRulesButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    paintedCountLabel.SetPos(XMFLOAT2(padding, y));
    paintedCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding * 2.0f;

    // Quality Controls
    qualityLabel.SetPos(XMFLOAT2(padding, y));
    qualityLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    qualityEnableCheck.SetPos(XMFLOAT2(padding, y));
    qualityEnableCheck.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    qualityPresetCombo.SetPos(XMFLOAT2(padding, y));
    qualityPresetCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    qualityDensityLabel.SetPos(XMFLOAT2(padding, y));
    qualityDensityLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    qualityFrequencyLabel.SetPos(XMFLOAT2(padding, y));
    qualityFrequencyLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    qualityMaxLayersLabel.SetPos(XMFLOAT2(padding, y));
    qualityMaxLayersLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding * 2.0f;

    // Bake Tools
    bakeLabel.SetPos(XMFLOAT2(padding, y));
    bakeLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    bakeTemplateCombo.SetPos(XMFLOAT2(padding, y));
    bakeTemplateCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    bakeButton.SetPos(XMFLOAT2(padding, y));
    bakeButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    bakeAllButton.SetPos(XMFLOAT2(padding, y));
    bakeAllButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    bakeStatusLabel.SetPos(XMFLOAT2(padding, y));
    bakeStatusLabel.SetSize(XMFLOAT2(contentW, labelH));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DMOProceduralTexturingWindow::RefreshTemplateInspector()
{
    static const char* names[] = { "DungeonWall", "ForestFloor", "MetalArmor", "CreatureLeather" };
    static const int layerCounts[] = { 2, 3, 3, 3 };
    static const bool neural[] = { false, true, true, true };
    static const char* layerDetails[] = {
        "Rust (18.0 Hz), Moss (12.0 Hz)",
        "Perlin (12.0 Hz), Moss (14.0 Hz), Voronoi (22.0 Hz)",
        "Rust (22.0 Hz), Cracks (9.0 Hz), Rust (28.0 Hz)",
        "WoodGrain (14.0 Hz), Cracks (9.0 Hz), Rust (22.0 Hz)",
    };

    int sel = templateTypeCombo.GetSelected();
    if (sel < 0 || sel >= 4) sel = 0;

    templateNameLabel.SetText(std::string("Name: ") + names[sel]);
    templateLayerCountLabel.SetText("Layers: " + std::to_string(layerCounts[sel]));
    templateNeuralLabel.SetText(std::string("Neural augmentation: ") + (neural[sel] ? "Yes" : "No"));
    templateLayersDetailLabel.SetText(layerDetails[sel]);
}

void DMOProceduralTexturingWindow::RefreshRuleInfo()
{
    static const int patternCounts[] = { 2, 2, 1, 1, 1, 1, 1 }; // matches default rules

    int sel = ruleLayerCombo.GetSelected();
    if (sel < 0 || sel >= 7) sel = 0;

    rulePatternCountLabel.SetText("Patterns in rule: " + std::to_string(patternCounts[sel]));
}

void DMOProceduralTexturingWindow::RefreshQualityInfo()
{
    int sel = qualityPresetCombo.GetSelected();

    float density = 0.75f;
    float frequency = 0.9f;
    uint32_t maxLayers = 2;

    switch (sel) {
        case 0: // Low
            density = 0.4f; frequency = 0.6f; maxLayers = 1;
            break;
        case 1: // Medium
            density = 0.75f; frequency = 0.9f; maxLayers = 2;
            break;
        case 2: // High
            density = 1.0f; frequency = 1.0f; maxLayers = 8;
            break;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Density scale: %.2f", density);
    qualityDensityLabel.SetText(buf);

    snprintf(buf, sizeof(buf), "Frequency scale: %.2f", frequency);
    qualityFrequencyLabel.SetText(buf);

    qualityMaxLayersLabel.SetText("Max layers/template: " + std::to_string(maxLayers));
}
