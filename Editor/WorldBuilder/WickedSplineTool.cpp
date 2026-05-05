#include "stdafx.h"
#include "WickedSplineTool.h"

void WickedSplineWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Spline Construction Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;

    addControlPointButton.Create("Add Control Point");
    addControlPointButton.SetPos(XMFLOAT2(10, y));
    addControlPointButton.SetSize(XMFLOAT2(200, 25));
    addControlPointButton.OnClick([this](wi::gui::EventArgs) { AddControlPoint(); });
    AddWidget(&addControlPointButton);
    y += 25 + padding;

    tensionSlider.Create(0.0f, 1.0f, 0.5f, 100, "Tension");
    tensionSlider.SetPos(XMFLOAT2(80, y));
    tensionSlider.SetSize(XMFLOAT2(130, 20));
    AddWidget(&tensionSlider);
    y += 20 + padding;

    SetSize(XMFLOAT2(230, y + 20));
    SetVisible(false);
}

void WickedSplineWindow::ResizeLayout() { wi::gui::Window::ResizeLayout(); }
void WickedSplineWindow::Update(const wi::Canvas& canvas, float dt) { wi::gui::Window::Update(canvas, dt); }

void WickedSplineWindow::AddControlPoint()
{
    wi::backlog::post("[WICKED-WORLDBUILD-09] Spline control point added.");
}
