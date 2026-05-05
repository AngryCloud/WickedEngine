// ===============================================
// WICKED-WORLDBUILD-06: Auto-Landscape Material Integration
// ===============================================
#pragma once
#include "WickedEngine.h"

class EditorComponent;

class WickedAutoLandscapeWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    
    EditorComponent* editor = nullptr;

    wi::gui::Button applyMaterialButton;
    wi::gui::Slider slopeTilingSlider;
    wi::gui::Slider heightBlendSlider;

    void ApplyLandscapeMaterial();
};
