#pragma once
#include "WickedEngine.h"

class EditorComponent;

class WickedCavePCGWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    
    EditorComponent* editor = nullptr;

    wi::gui::Button generateCaveButton;
    wi::gui::Slider complexitySlider;

    void GenerateCaveSystem();
};
