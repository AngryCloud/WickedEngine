#pragma once
// Editor/DMONeuralShaderWindow.h
// DMO Neural Shader editor panel — material inspector, network visualizer,
// live preview controls, and bake tools for the WICKED-NEURAL subsystem.
//
// Follows the wi::gui::Window pattern of DMOAssetPlacerWindow / PaintToolWindow.
// Lives exclusively in the Editor target — never compiled into client builds.

#include "wiGUI.h"
#include "wiScene.h"
#include "wiECS.h"
#include <string>

class EditorComponent;

class DMONeuralShaderWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;

    EditorComponent* editor = nullptr;

private:
    // ---- Section: Material Inspector ----
    wi::gui::Label    matInspectorLabel;
    wi::gui::ComboBox matTypeCombo;
    wi::gui::Label    matPathLabel;
    wi::gui::Label    matInputDimLabel;
    wi::gui::Label    matOutputDimLabel;
    wi::gui::Label    matLayerCountLabel;
    wi::gui::Label    matWeightCountLabel;
    wi::gui::Button   matAssignButton;

    // ---- Section: Network Visualizer ----
    wi::gui::Label    netVisualizerLabel;
    wi::gui::Label    netArchLabel;         // "8 -> 32 -> 32 -> 32 -> 6"
    wi::gui::Label    netParamCountLabel;
    wi::gui::Label    netDispatchCountLabel;
    wi::gui::Button   netResetStatsButton;

    // ---- Section: Quality & Live Preview ----
    wi::gui::Label    previewLabel;
    wi::gui::CheckBox previewEnableCheck;
    wi::gui::ComboBox previewQualityCombo;
    wi::gui::Label    previewBatchLabel;
    wi::gui::Label    previewScaleLabel;
    wi::gui::Label    previewRefinerLabel;

    // ---- Section: Bake Tools ----
    wi::gui::Label    bakeLabel;
    wi::gui::ComboBox bakeMaterialCombo;
    wi::gui::Button   bakeButton;
    wi::gui::Button   bakeAllButton;
    wi::gui::Label    bakeStatusLabel;

    // Helpers
    void RefreshInspector();
    void RefreshNetworkVis();
    void RefreshPreviewInfo();
    int  GetSelectedMaterialIndex() const;
};
