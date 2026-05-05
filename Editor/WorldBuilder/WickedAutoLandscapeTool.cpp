#include "stdafx.h"
#include "WickedAutoLandscapeTool.h"

void WickedAutoLandscapeWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Auto-Landscape Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;

    applyMaterialButton.Create("Apply Landscape Material");
    applyMaterialButton.SetPos(XMFLOAT2(10, y));
    applyMaterialButton.SetSize(XMFLOAT2(200, 25));
    applyMaterialButton.OnClick([this](wi::gui::EventArgs) { ApplyLandscapeMaterial(); });
    AddWidget(&applyMaterialButton);
    y += 25 + padding;

    slopeTilingSlider.Create(1.0f, 100.0f, 50.0f, 1000, "Slope Tiling");
    slopeTilingSlider.SetPos(XMFLOAT2(80, y));
    slopeTilingSlider.SetSize(XMFLOAT2(130, 20));
    AddWidget(&slopeTilingSlider);
    y += 20 + padding;

    heightBlendSlider.Create(0.0f, 1.0f, 0.5f, 100, "Height Blend");
    heightBlendSlider.SetPos(XMFLOAT2(80, y));
    heightBlendSlider.SetSize(XMFLOAT2(130, 20));
    AddWidget(&heightBlendSlider);
    y += 20 + padding;

    SetSize(XMFLOAT2(230, y + 20));
    SetVisible(false);
}

void WickedAutoLandscapeWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
}

void WickedAutoLandscapeWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);
}

void WickedAutoLandscapeWindow::ApplyLandscapeMaterial()
{
    wi::backlog::post("[WICKED-WORLDBUILD-06] Auto-Landscape Material settings applied!");
}
