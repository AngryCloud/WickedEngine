#pragma once
#include "WickedEngine.h"

class EditorComponent;

class WickedSplineWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    
    EditorComponent* editor = nullptr;

    wi::gui::Button addControlPointButton;
    wi::gui::Slider tensionSlider;

    void AddControlPoint();
};
