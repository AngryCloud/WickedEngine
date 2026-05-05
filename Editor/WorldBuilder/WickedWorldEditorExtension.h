// ===============================================
// WICKED-WORLDBUILD-01: Core World Editor Extension Framework
// ===============================================
#pragma once

#include "WickedEngine.h"

class EditorComponent;

// ------------------------------------------------------------------
// WickedWorldEditorHubWindow (main editor hub)
// ------------------------------------------------------------------
class WickedWorldEditorHubWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

    // UI Widgets
    wi::gui::Button openMasterControlsButton;
    wi::gui::Button saveSpawnPointButton;
    wi::gui::Button playFromHereButton;
    
    // Future tool shortcuts
    wi::gui::Button openSplineToolButton;
    wi::gui::Button openCaveToolButton;
    wi::gui::Button openForestToolButton;
    wi::gui::Button openDitchStreamButton;
    wi::gui::Button openLandscapeButton;

    // Play-From-Here
    void PlayFromHere(const XMFLOAT3& worldPos, float yaw = 0.0f);
    void SaveSpawnPoint(const XMFLOAT3& worldPos, float yaw);

private:
    // Saved spawn point
    XMFLOAT3 m_savedSpawnPos = { 0, 0, 0 };
    float    m_savedSpawnYaw = 0.0f;

    // Telemetry
    float m_timeSinceTelemetry = 0.0f;
};
