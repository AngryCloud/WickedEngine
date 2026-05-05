#include "stdafx.h"
#include "WickedDitchStreamTool.h"

void WickedDitchStreamWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("Ditch & Stream Tool", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;

    generateStreamButton.Create("Carve Stream Ditch");
    generateStreamButton.SetPos(XMFLOAT2(10, y));
    generateStreamButton.SetSize(XMFLOAT2(200, 25));
    generateStreamButton.OnClick([this](wi::gui::EventArgs) { GenerateStream(); });
    AddWidget(&generateStreamButton);
    y += 25 + padding;

    streamWidthSlider.Create(1.0f, 20.0f, 5.0f, 100, "Width");
    streamWidthSlider.SetPos(XMFLOAT2(80, y));
    streamWidthSlider.SetSize(XMFLOAT2(130, 20));
    AddWidget(&streamWidthSlider);
    y += 20 + padding;

    SetSize(XMFLOAT2(230, y + 20));
    SetVisible(false);
}

void WickedDitchStreamWindow::ResizeLayout() { wi::gui::Window::ResizeLayout(); }
void WickedDitchStreamWindow::Update(const wi::Canvas& canvas, float dt) { wi::gui::Window::Update(canvas, dt); }

void WickedDitchStreamWindow::GenerateStream()
{
    wi::backlog::post("[WICKED-WORLDBUILD-04] Stream network carved.");
}
