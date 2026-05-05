// ===============================================
// WICKED-WORLDBUILD-08: Exclusion Zones + Final Integration & Polish
// ===============================================
#pragma once

#include "WickedEngine.h"
#include <vector>
#include <string>

class EditorComponent;

// ------------------------------------------------------------------
// Exclusion Zone (paintable volume)
// ------------------------------------------------------------------
struct ExclusionZone
{
    enum class Type { Sphere, Box, SplineVolume };

    Type type = Type::Sphere;
    XMFLOAT3 center = { 0, 0, 0 };
    XMFLOAT3 extent = { 20, 20, 20 };   // for box / radius for sphere
    std::vector<XMFLOAT3> splinePoints; // for spline volume
    std::string reason;                 // e.g. "No trees here"
};

// ------------------------------------------------------------------
// WickedWorldBuilderMasterWindow (final integration hub)
// ------------------------------------------------------------------
class WickedWorldBuilderMasterWindow : public wi::gui::Window
{
public:
    void Create(EditorComponent* editor);

    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;

    EditorComponent* editor = nullptr;

    // UI Widgets
    wi::gui::Button bakeButton;
    wi::gui::Button validateButton;
    wi::gui::Button paintSphereButton;
    wi::gui::Button paintBoxButton;
    wi::gui::Button clearExclusionsButton;
    wi::gui::Label  validationResultsLabel;

    // Master actions
    void BakeAllToDataLayers();
    void RunValidationSuite();
    void ClearAllExclusionZones();

    // Exclusion painting
    void StartPaintingExclusion(ExclusionZone::Type type);
    void AddExclusionPoint(const XMFLOAT3& worldPos);

private:
    std::vector<ExclusionZone> m_exclusionZones;

    bool m_isPaintingExclusion = false;
    ExclusionZone::Type m_paintType = ExclusionZone::Type::Sphere;

    // Validation results
    struct ValidationResult
    {
        std::string message;
        bool isError = false;
    };
    std::vector<ValidationResult> m_lastValidation;

    void RefreshValidationLabel();
};
