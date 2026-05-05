#pragma once
#include "WickedEngine.h"

class EditorComponent;

class WickedModularCaveWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    
    EditorComponent* editor = nullptr;

    wi::gui::Button addCaveModuleButton;
    wi::gui::CheckBox snapToGridCheck;

    void AddCaveModule();
};
