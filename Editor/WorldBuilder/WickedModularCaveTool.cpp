#include "stdafx.h"
#include "WickedModularCaveTool.h"

void WickedModularCaveWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Modular Cave Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;

    addCaveModuleButton.Create("Spawn Cave Module");
    addCaveModuleButton.SetPos(XMFLOAT2(10, y));
    addCaveModuleButton.SetSize(XMFLOAT2(200, 25));
    addCaveModuleButton.OnClick([this](wi::gui::EventArgs) { AddCaveModule(); });
    AddWidget(&addCaveModuleButton);
    y += 25 + padding;

    snapToGridCheck.Create("Grid Snap");
    snapToGridCheck.SetCheck(true);
    snapToGridCheck.SetPos(XMFLOAT2(100, y));
    snapToGridCheck.SetSize(XMFLOAT2(20, 20));
    AddWidget(&snapToGridCheck);
    y += 20 + padding;

    SetSize(XMFLOAT2(230, y + 20));
    SetVisible(false);
}

void WickedModularCaveWindow::ResizeLayout() { wi::gui::Window::ResizeLayout(); }
void WickedModularCaveWindow::Update(const wi::Canvas& canvas, float dt) { wi::gui::Window::Update(canvas, dt); }

void WickedModularCaveWindow::AddCaveModule()
{
    wi::backlog::post("[WICKED-WORLDBUILD-02] Spawned modular cave room.");
}
