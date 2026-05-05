#pragma once
// Editor/DMOProceduralTexturingWindow.h
// DMO Procedural Texturing editor panel — rule painter, template inspector,
// quality controls, live preview, and bake tools.
//
// Follows the wi::gui::Window pattern of DMOAssetPlacerWindow / DMONeuralShaderWindow.
// Lives exclusively in the Editor target — never compiled into client builds.

#include "wiGUI.h"
#include "wiScene.h"
#include "wiECS.h"
#include <string>

class EditorComponent;

class DMOProceduralTexturingWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;

    EditorComponent* editor = nullptr;

private:
    // ---- Section: Template Inspector ----
    wi::gui::Label    templateInspectorLabel;
    wi::gui::ComboBox templateTypeCombo;
    wi::gui::Label    templateNameLabel;
    wi::gui::Label    templateLayerCountLabel;
    wi::gui::Label    templateNeuralLabel;
    wi::gui::Label    templateLayersDetailLabel;

    // ---- Section: Rule Painter ----
    wi::gui::Label    rulePainterLabel;
    wi::gui::ComboBox ruleLayerCombo;
    wi::gui::Label    rulePatternCountLabel;
    wi::gui::Button   paintRuleButton;
    wi::gui::Button   clearEntityRulesButton;
    wi::gui::Label    paintedCountLabel;

    // ---- Section: Quality Controls ----
    wi::gui::Label    qualityLabel;
    wi::gui::ComboBox qualityPresetCombo;
    wi::gui::Label    qualityDensityLabel;
    wi::gui::Label    qualityFrequencyLabel;
    wi::gui::Label    qualityMaxLayersLabel;
    wi::gui::CheckBox qualityEnableCheck;

    // ---- Section: Bake Tools ----
    wi::gui::Label    bakeLabel;
    wi::gui::ComboBox bakeTemplateCombo;
    wi::gui::Button   bakeButton;
    wi::gui::Button   bakeAllButton;
    wi::gui::Label    bakeStatusLabel;

    // Helpers
    void RefreshTemplateInspector();
    void RefreshRuleInfo();
    void RefreshQualityInfo();
};
