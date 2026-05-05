#include "stdafx.h"
#include "WickedForestPCGTool.h"

// ------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------
void WickedForestPCGWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Advanced PCG Forest System", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    // Set up mock rules (ideally pulled from Registry)
    m_availableBiomes = {
        {"TemperateForest", {"oak_tree.glb", "birch_tree.glb"}, {"fern.glb", "bush.glb"}, 0.14f, 0.5f, 0.8f, 1.7f, 35.0f, 0.0f, 9999.0f},
        {"PineHills",       {"pine_tree.glb"}, {"rock_small.glb", "moss_patch.glb"}, 0.09f, 0.35f, 0.9f, 1.4f, 42.0f, 80.0f, 300.0f},
        {"AncientGrove",    {"ancient_tree.glb"}, {"mushroom_tree.glb"}, {"ivy.glb"}, 0.07f, 0.65f, 1.2f, 2.2f, 25.0f, 0.0f, 120.0f}
    };

    float y = 10;
    float padding = 5;

    biomeCombo.Create("Biome");
    biomeCombo.SetPos(XMFLOAT2(100, y));
    biomeCombo.SetSize(XMFLOAT2(150, 20));
    for (const auto& biome : m_availableBiomes) {
        biomeCombo.AddItem(biome.biomeName);
    }
    AddWidget(&biomeCombo);
    y += 20 + padding;

    radiusSlider.Create(40.0f, 300.0f, 120.0f, 100, "Radius");
    radiusSlider.SetPos(XMFLOAT2(100, y));
    radiusSlider.SetSize(XMFLOAT2(150, 20));
    AddWidget(&radiusSlider);
    y += 20 + padding;

    autoBakeCheck.Create("Auto-Bake");
    autoBakeCheck.SetCheck(true);
    autoBakeCheck.SetPos(XMFLOAT2(100, y));
    autoBakeCheck.SetSize(XMFLOAT2(20, 20));
    AddWidget(&autoBakeCheck);
    y += 20 + padding;

    generateButton.Create("Generate Forest in Area");
    generateButton.SetPos(XMFLOAT2(10, y));
    generateButton.SetSize(XMFLOAT2(240, 25));
    generateButton.OnClick([this](wi::gui::EventArgs) { GenerateForestOnCurrentSelection(); });
    AddWidget(&generateButton);
    y += 25 + padding;

    SetVisible(false);
    SetSize(XMFLOAT2(260, y + 20));
}

void WickedForestPCGWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
}

void WickedForestPCGWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);
}

void WickedForestPCGWindow::GenerateForestOnCurrentSelection()
{
    int selectedIdx = std::max(0, biomeCombo.GetSelected());
    if (m_availableBiomes.empty()) return;
    
    const BiomeForestRule& rule = m_availableBiomes[selectedIdx];

    // Mock generation execution
    XMFLOAT3 center = {0, 0, 0}; 
    float radius = radiusSlider.GetValue();

    PCGForestComponent forest;
    forest.activeRule = rule;
    forest.GenerateInArea(center, radius);

    if (autoBakeCheck.GetCheck())
        forest.BakeToDataLayer();

    wi::backlog::post("[WICKED-WORLDBUILD-05] Generated forest area!");
}

// ------------------------------------------------------------------
// PCGForestComponent implementation
// ------------------------------------------------------------------
void PCGForestComponent::GenerateInArea(const XMFLOAT3& center, float radius)
{
    // Minimal mock to replace original pseudocode
    int treeCount = static_cast<int>(activeRule.treeDensityPerSqMeter * (radius * radius * 3.14f));
    for (int i = 0; i < treeCount; ++i)
    {
        wi::ecs::Entity tree = wi::ecs::CreateEntity();
        spawnedTrees.push_back(tree);
    }
}

void PCGForestComponent::BakeToDataLayer()
{
    wi::backlog::post("[WICKED-WORLDBUILD-05] Forest baked to streaming Data Layer");
}
