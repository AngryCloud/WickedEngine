// Editor/DMONeuralShaderWindow.cpp
// DMO Neural Shader editor panel — WICKED-NEURAL subsystem tools.

#include "stdafx.h"
#include "DMONeuralShaderWindow.h"
#include "Editor.h"

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

void DMONeuralShaderWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Neural Shader", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(340, 580));

    // ================================================================
    // Material Inspector
    // ================================================================
    matInspectorLabel.Create("--- Material Inspector ---");
    matInspectorLabel.SetColor(wi::Color(200, 200, 255, 255));
    AddWidget(&matInspectorLabel);

    matTypeCombo.Create("Material: ");
    matTypeCombo.AddItem("Skin");
    matTypeCombo.AddItem("Cloth");
    matTypeCombo.AddItem("Metal");
    matTypeCombo.AddItem("Organic");
    matTypeCombo.SetSelected(0);
    matTypeCombo.SetTooltip("Select a neural material type to inspect");
    matTypeCombo.OnSelect([this](wi::gui::EventArgs args) {
        RefreshInspector();
        RefreshNetworkVis();
    });
    AddWidget(&matTypeCombo);

    matPathLabel.Create("Path: assets/neural/material_skin_fp16.mlp");
    AddWidget(&matPathLabel);

    matInputDimLabel.Create("Input dim: 8");
    AddWidget(&matInputDimLabel);

    matOutputDimLabel.Create("Output dim: 6");
    AddWidget(&matOutputDimLabel);

    matLayerCountLabel.Create("Layers: 4");
    AddWidget(&matLayerCountLabel);

    matWeightCountLabel.Create("Weights: --");
    AddWidget(&matWeightCountLabel);

    matAssignButton.Create("Assign to Selected Entity");
    matAssignButton.SetTooltip("Apply this neural material to the currently selected entity in the scene");
    matAssignButton.OnClick([this](wi::gui::EventArgs args) {
        // Future: assign neural material override to selected entity's material component
    });
    AddWidget(&matAssignButton);

    // ================================================================
    // Network Visualizer
    // ================================================================
    netVisualizerLabel.Create("--- Network Visualizer ---");
    netVisualizerLabel.SetColor(wi::Color(200, 255, 200, 255));
    AddWidget(&netVisualizerLabel);

    netArchLabel.Create("Arch: 8 -> 32 -> 32 -> 32 -> 6");
    netArchLabel.SetTooltip("Input -> Hidden layers -> Output dimensions");
    AddWidget(&netArchLabel);

    netParamCountLabel.Create("Parameters: --");
    AddWidget(&netParamCountLabel);

    netDispatchCountLabel.Create("Dispatches: 0");
    AddWidget(&netDispatchCountLabel);

    netResetStatsButton.Create("Reset Dispatch Stats");
    netResetStatsButton.SetTooltip("Zero out the dispatch counters for all material MLPs");
    netResetStatsButton.OnClick([this](wi::gui::EventArgs args) {
        netDispatchCountLabel.SetText("Dispatches: 0");
    });
    AddWidget(&netResetStatsButton);

    // ================================================================
    // Quality & Live Preview
    // ================================================================
    previewLabel.Create("--- Quality & Live Preview ---");
    previewLabel.SetColor(wi::Color(255, 255, 200, 255));
    AddWidget(&previewLabel);

    previewEnableCheck.Create("Neural Shading Enabled");
    previewEnableCheck.SetCheck(true);
    previewEnableCheck.SetTooltip("Master enable/disable for the neural shading subsystem");
    previewEnableCheck.OnClick([this](wi::gui::EventArgs args) {
        RefreshPreviewInfo();
    });
    AddWidget(&previewEnableCheck);

    previewQualityCombo.Create("Quality: ");
    previewQualityCombo.AddItem("Low");
    previewQualityCombo.AddItem("Medium");
    previewQualityCombo.AddItem("High");
    previewQualityCombo.SetSelected(1); // Medium default
    previewQualityCombo.SetTooltip("Neural shading quality preset — controls batch size, evaluation scale, and probe refiner frequency");
    previewQualityCombo.OnSelect([this](wi::gui::EventArgs args) {
        RefreshPreviewInfo();
    });
    AddWidget(&previewQualityCombo);

    previewBatchLabel.Create("Batch size: 512");
    AddWidget(&previewBatchLabel);

    previewScaleLabel.Create("Eval scale: 0.85");
    AddWidget(&previewScaleLabel);

    previewRefinerLabel.Create("Probe refiner freq: 3");
    AddWidget(&previewRefinerLabel);

    // ================================================================
    // Bake Tools
    // ================================================================
    bakeLabel.Create("--- Bake Tools ---");
    bakeLabel.SetColor(wi::Color(255, 200, 200, 255));
    AddWidget(&bakeLabel);

    bakeMaterialCombo.Create("Bake Target: ");
    bakeMaterialCombo.AddItem("Skin");
    bakeMaterialCombo.AddItem("Cloth");
    bakeMaterialCombo.AddItem("Metal");
    bakeMaterialCombo.AddItem("Organic");
    bakeMaterialCombo.SetSelected(0);
    bakeMaterialCombo.SetTooltip("Material type to bake into a lookup texture");
    AddWidget(&bakeMaterialCombo);

    bakeButton.Create("Bake Selected");
    bakeButton.SetTooltip("Bake the selected material MLP into a precomputed lookup texture");
    bakeButton.OnClick([this](wi::gui::EventArgs args) {
        int sel = bakeMaterialCombo.GetSelected();
        std::string name = (sel >= 0) ? bakeMaterialCombo.GetItemText(sel) : "Unknown";
        bakeStatusLabel.SetText("Baking " + name + "...");
        // Future: dispatch offline bake pass through WickedNeuralShading
    });
    AddWidget(&bakeButton);

    bakeAllButton.Create("Bake All Materials");
    bakeAllButton.SetTooltip("Bake all four material MLPs into precomputed lookup textures");
    bakeAllButton.OnClick([this](wi::gui::EventArgs args) {
        bakeStatusLabel.SetText("Baking all materials...");
        // Future: iterate all material types and dispatch bake
    });
    AddWidget(&bakeAllButton);

    bakeStatusLabel.Create("Status: Idle");
    AddWidget(&bakeStatusLabel);

    SetVisible(false);

    // Populate initial values
    RefreshInspector();
    RefreshNetworkVis();
    RefreshPreviewInfo();
}

// ---------------------------------------------------------------------------
// ResizeLayout
// ---------------------------------------------------------------------------

void DMONeuralShaderWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();

    const float padding = 4.0f;
    const float itemH   = 18.0f;
    const float labelH  = 16.0f;
    const float winW    = GetSize().x;
    const float contentW = winW - padding * 2.0f;

    float y = padding;

    // Material Inspector section
    matInspectorLabel.SetPos(XMFLOAT2(padding, y));
    matInspectorLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    matTypeCombo.SetPos(XMFLOAT2(padding, y));
    matTypeCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    matPathLabel.SetPos(XMFLOAT2(padding, y));
    matPathLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    matInputDimLabel.SetPos(XMFLOAT2(padding, y));
    matInputDimLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    matOutputDimLabel.SetPos(XMFLOAT2(padding, y));
    matOutputDimLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    matLayerCountLabel.SetPos(XMFLOAT2(padding, y));
    matLayerCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    matWeightCountLabel.SetPos(XMFLOAT2(padding, y));
    matWeightCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    matAssignButton.SetPos(XMFLOAT2(padding, y));
    matAssignButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding * 2.0f;

    // Network Visualizer section
    netVisualizerLabel.SetPos(XMFLOAT2(padding, y));
    netVisualizerLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    netArchLabel.SetPos(XMFLOAT2(padding, y));
    netArchLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    netParamCountLabel.SetPos(XMFLOAT2(padding, y));
    netParamCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    netDispatchCountLabel.SetPos(XMFLOAT2(padding, y));
    netDispatchCountLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    netResetStatsButton.SetPos(XMFLOAT2(padding, y));
    netResetStatsButton.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding * 2.0f;

    // Quality & Live Preview section
    previewLabel.SetPos(XMFLOAT2(padding, y));
    previewLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    previewEnableCheck.SetPos(XMFLOAT2(padding, y));
    previewEnableCheck.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    previewQualityCombo.SetPos(XMFLOAT2(padding, y));
    previewQualityCombo.SetSize(XMFLOAT2(contentW, itemH));
    y += itemH + padding;

    previewBatchLabel.SetPos(XMFLOAT2(padding, y));
    previewBatchLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    previewScaleLabel.SetPos(XMFLOAT2(padding, y));
    previewScaleLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + 2.0f;

    previewRefinerLabel.SetPos(XMFLOAT2(padding, y));
    previewRefinerLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding * 2.0f;

    // Bake Tools section
    bakeLabel.SetPos(XMFLOAT2(padding, y));
    bakeLabel.SetSize(XMFLOAT2(contentW, labelH));
    y += labelH + padding;

    bakeMaterialCombo.SetPos(XMFLOAT2(padding, y));
    bakeMaterialCombo.SetSize(XMFLOAT2(contentW, itemH));
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

int DMONeuralShaderWindow::GetSelectedMaterialIndex() const
{
    return matTypeCombo.GetSelected();
}

void DMONeuralShaderWindow::RefreshInspector()
{
    // Material asset paths corresponding to MaterialType enum order
    static const char* paths[] = {
        "assets/neural/material_skin_fp16.mlp",
        "assets/neural/material_cloth_fp16.mlp",
        "assets/neural/material_metal_fp16.mlp",
        "assets/neural/material_organic_fp16.mlp",
    };
    // Default MLP dimensions (matches TinyMLP shim defaults)
    static const int inputDims[]  = { 8, 8, 8, 8 };
    static const int outputDims[] = { 6, 6, 6, 6 };
    static const int layerCounts[] = { 4, 4, 4, 4 };

    int sel = GetSelectedMaterialIndex();
    if (sel < 0 || sel >= 4) sel = 0;

    matPathLabel.SetText(std::string("Path: ") + paths[sel]);
    matInputDimLabel.SetText("Input dim: " + std::to_string(inputDims[sel]));
    matOutputDimLabel.SetText("Output dim: " + std::to_string(outputDims[sel]));
    matLayerCountLabel.SetText("Layers: " + std::to_string(layerCounts[sel]));

    // Weight count estimate: input*hidden + (numLayers-2)*hidden*hidden + hidden*output
    int hidden = 32;
    int nLayers = layerCounts[sel];
    int weights = inputDims[sel] * hidden;
    for (int i = 1; i < nLayers - 1; ++i)
        weights += hidden * hidden;
    weights += hidden * outputDims[sel];
    matWeightCountLabel.SetText("Weights: " + std::to_string(weights));
}

void DMONeuralShaderWindow::RefreshNetworkVis()
{
    int sel = GetSelectedMaterialIndex();
    if (sel < 0 || sel >= 4) sel = 0;

    // Architecture string: input -> hidden layers -> output
    int inputDim = 8, hiddenDim = 32, outputDim = 6, numLayers = 4;
    std::string arch = std::to_string(inputDim);
    for (int i = 0; i < numLayers - 1; ++i)
        arch += " -> " + std::to_string(hiddenDim);
    arch += " -> " + std::to_string(outputDim);
    netArchLabel.SetText("Arch: " + arch);

    // Parameter count (weights + biases)
    int weights = inputDim * hiddenDim;
    int biases = hiddenDim;
    for (int i = 1; i < numLayers - 1; ++i) {
        weights += hiddenDim * hiddenDim;
        biases += hiddenDim;
    }
    weights += hiddenDim * outputDim;
    biases += outputDim;
    netParamCountLabel.SetText("Parameters: " + std::to_string(weights + biases));
}

void DMONeuralShaderWindow::RefreshPreviewInfo()
{
    int sel = previewQualityCombo.GetSelected();

    // Mirror QualityController preset values
    uint32_t batchSize = 512;
    float evalScale = 0.85f;
    uint32_t refinerFreq = 3;

    switch (sel) {
        case 0: // Low
            batchSize = 128; evalScale = 0.5f; refinerFreq = 8;
            break;
        case 1: // Medium
            batchSize = 512; evalScale = 0.85f; refinerFreq = 3;
            break;
        case 2: // High
            batchSize = 1024; evalScale = 1.0f; refinerFreq = 1;
            break;
    }

    previewBatchLabel.SetText("Batch size: " + std::to_string(batchSize));

    // Format eval scale with 2 decimal places
    char scaleBuf[32];
    snprintf(scaleBuf, sizeof(scaleBuf), "Eval scale: %.2f", evalScale);
    previewScaleLabel.SetText(scaleBuf);

    previewRefinerLabel.SetText("Probe refiner freq: " + std::to_string(refinerFreq));
}
