#pragma once

#include "WickedEngine.h"

// Editor/RiverSplineWindow.h
// Provides a docking panel to import UE river spline json definitions,
// generate a ribbon mesh through the control points, and manipulate it.

class EditorComponent;

class RiverSplineWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

    // UI Elements
    wi::gui::Button loadButton;
    wi::gui::Button clearButton;
    wi::gui::Slider widthSlider;
    wi::gui::Label statusLabel;

private:
    struct SplinePoint
    {
        XMFLOAT3 position;
        XMFLOAT3 arriveTangent;
        XMFLOAT3 leaveTangent;
    };

    std::vector<SplinePoint> m_points;
    wi::ecs::Entity m_riverEntity = wi::ecs::INVALID_ENTITY;

    void BuildRiverMesh();
    void SetStatus(const std::string& msg);
};
