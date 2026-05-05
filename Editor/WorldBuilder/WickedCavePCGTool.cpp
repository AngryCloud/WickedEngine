#include "stdafx.h"
#include "WickedCavePCGTool.h"

void WickedCavePCGWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("PCG Cave Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;

    generateCaveButton.Create("Generate Cave Nodes");
    generateCaveButton.SetPos(XMFLOAT2(10, y));
    generateCaveButton.SetSize(XMFLOAT2(200, 25));
    generateCaveButton.OnClick([this](wi::gui::EventArgs) { GenerateCaveSystem(); });
    AddWidget(&generateCaveButton);
    y += 25 + padding;

    complexitySlider.Create(1.0f, 10.0f, 5.0f, 100, "Complexity");
    complexitySlider.SetPos(XMFLOAT2(80, y));
    complexitySlider.SetSize(XMFLOAT2(130, 20));
    AddWidget(&complexitySlider);
    y += 20 + padding;

    SetSize(XMFLOAT2(230, y + 20));
    SetVisible(false);
}

void WickedCavePCGWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
}

void WickedCavePCGWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);
}

void WickedCavePCGWindow::GenerateCaveSystem()
{
    wi::backlog::post("[WICKED-WORLDBUILD-03] Cave generated.");
}
