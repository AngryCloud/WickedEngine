#pragma once
// Editor/DMOAssetPlacerWindow.h
// DMO Asset Placer panel — surface-snap, grid, brush, and free placement modes
// with ghost preview, randomizers, and undo support.
//
// Follows the wi::gui::Window pattern of PaintToolWindow / TerrainWindow.
// Lives exclusively in the Editor target — never compiled into client builds.

#include "wiGUI.h"
#include "wiScene.h"
#include "wiECS.h"
#include "wiVector.h"
#include <string>

class EditorComponent;

class DMOAssetPlacerWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

private:
    enum class PlacementMode { Free, Grid, SurfaceSnap, Brush };
    PlacementMode m_mode = PlacementMode::Free;

    wi::ecs::Entity m_ghostEntity = wi::ecs::INVALID_ENTITY;
    std::string     m_selectedAssetPath;

    // Per-stroke accumulator for brush undo
    wi::vector<wi::ecs::Entity> m_strokeEntities;

    // Scroll-wheel overrides (applied on top of randomizers)
    float m_scrollYRotation = 0.0f;
    float m_scrollHeightOfs = 0.0f;
    float m_scrollScale     = 1.0f;

    // UI widgets
    wi::gui::ComboBox modeCombo;
    wi::gui::Slider   gridSizeSlider;
    wi::gui::Slider   brushRadiusSlider;
    wi::gui::Slider   brushDensitySlider;
    wi::gui::Slider   scaleMinSlider;
    wi::gui::Slider   scaleMaxSlider;
    wi::gui::Slider   rotYRangeSlider;
    wi::gui::Slider   tiltRangeSlider;

    // Helpers
    void     UpdateGhostEntity(const XMFLOAT3& worldPos, const XMFLOAT3& normal);
    void     HideGhostEntity();
    void     RemoveGhostEntity();
    XMFLOAT3 SnapToGrid(const XMFLOAT3& worldPos, float gridSize) const;
    int      ComputeBrushCount(float density, float radius) const;
    XMFLOAT3 ApplyRandomizers(const XMFLOAT3& basePos, const XMFLOAT3& normal) const;
    void     PlaceInstance(const XMFLOAT3& pos, const XMFLOAT3& normal, bool recordHistory);
    void     CommitBrushStroke();
};
