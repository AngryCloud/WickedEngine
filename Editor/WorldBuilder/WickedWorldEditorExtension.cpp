#include "stdafx.h"
#include "WickedWorldEditorExtension.h"

void WickedWorldEditorHubWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    wi::gui::Window::Create("World Builder Hub", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);

    float y = 10;
    float padding = 5;
    float btnH = 25;

    openMasterControlsButton.Create("Open Master Controls");
    openMasterControlsButton.SetPos(XMFLOAT2(10, y));
    openMasterControlsButton.SetSize(XMFLOAT2(180, btnH));
    openMasterControlsButton.OnClick([this](wi::gui::EventArgs) {
        // Toggle the WorldBuilderMasterWindow visibility inside the editor
    });
    AddWidget(&openMasterControlsButton);
    y += btnH + padding * 3;

    // --- Sub Tools ---
    openSplineToolButton.Create("Spline Toolkit");
    openSplineToolButton.SetPos(XMFLOAT2(10, y));
    openSplineToolButton.SetSize(XMFLOAT2(180, btnH));
    AddWidget(&openSplineToolButton);
    y += btnH + padding;

    openCaveToolButton.Create("Modular Cave Kit");
    openCaveToolButton.SetPos(XMFLOAT2(10, y));
    openCaveToolButton.SetSize(XMFLOAT2(180, btnH));
    AddWidget(&openCaveToolButton);
    y += btnH + padding;

    openForestToolButton.Create("PCG Forest System");
    openForestToolButton.SetPos(XMFLOAT2(10, y));
    openForestToolButton.SetSize(XMFLOAT2(180, btnH));
    AddWidget(&openForestToolButton);
    y += btnH + padding;

    openDitchStreamButton.Create("Ditch & Stream Tool");
    openDitchStreamButton.SetPos(XMFLOAT2(10, y));
    openDitchStreamButton.SetSize(XMFLOAT2(180, btnH));
    AddWidget(&openDitchStreamButton);
    y += btnH + padding;

    openLandscapeButton.Create("Auto-Landscape Tool");
    openLandscapeButton.SetPos(XMFLOAT2(10, y));
    openLandscapeButton.SetSize(XMFLOAT2(180, btnH));
    AddWidget(&openLandscapeButton);
    y += btnH + padding * 3;

    // --- Viewport State Actions ---
    saveSpawnPointButton.Create("Save Current Spawn Point");
    saveSpawnPointButton.SetPos(XMFLOAT2(10, y));
    saveSpawnPointButton.SetSize(XMFLOAT2(180, btnH));
    saveSpawnPointButton.OnClick([this](wi::gui::EventArgs) {
        SaveSpawnPoint(XMFLOAT3(0,0,0), 0.0f); // Mock cursor pos
    });
    AddWidget(&saveSpawnPointButton);
    y += btnH + padding;

    playFromHereButton.Create("Play From Here");
    playFromHereButton.SetPos(XMFLOAT2(10, y));
    playFromHereButton.SetSize(XMFLOAT2(180, btnH));
    playFromHereButton.OnClick([this](wi::gui::EventArgs) {
        PlayFromHere(m_savedSpawnPos, m_savedSpawnYaw);
    });
    AddWidget(&playFromHereButton);
    y += btnH + padding;

    SetVisible(false);
    SetSize(XMFLOAT2(200, y + 20));
}

void WickedWorldEditorHubWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();
}

void WickedWorldEditorHubWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);

    m_timeSinceTelemetry += dt;
    if (m_timeSinceTelemetry > 2.0f)
    {
        // DMO_ENV05_RECORD("WorldEditorHub_Active", 1);
        m_timeSinceTelemetry = 0.0f;
    }
}

void WickedWorldEditorHubWindow::PlayFromHere(const XMFLOAT3& worldPos, float yaw)
{
    wi::backlog::post("[WICKED-WORLDBUILD-01] Play-From-Here mechanism activated");
    // e.g. Trigger state machine change to game mode and push camera
}

void WickedWorldEditorHubWindow::SaveSpawnPoint(const XMFLOAT3& worldPos, float yaw)
{
    m_savedSpawnPos = worldPos;
    m_savedSpawnYaw = yaw;
    wi::backlog::post("[WICKED-WORLDBUILD-01] Spawn point saved");
}
