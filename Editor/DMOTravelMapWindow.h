#pragma once
// Editor/DMOTravelMapWindow.h
// DMO Travel Map Editor panel — dockable window for authoring zone anchors,
// city pins, and place-name labels on the Oracle map texture.
//
// Wraps WickedTravelMapEditorTool (pure data layer, no GPU resources).
// Follows the wi::gui::Window pattern of PaintToolWindow / TerrainWindow.
// Lives exclusively in the Editor target — never compiled into client builds.

class EditorComponent;

// Editor-only client data-layer bridge. This header is deliberately confined
// to the DMO editor target and must not be included from client runtime code.
#include "Travel/WickedTravelMapEditorTool.h"

struct WickedFloat2;
struct WickedZoneMapAnchorRecord;

class DMOTravelMapWindow : public wi::gui::Window
{
public:
    ~DMOTravelMapWindow();

    // Called from EditorComponent::Load() — matches the pattern of all other
    // editor windows (PaintToolWindow, TerrainWindow, etc.).
    void Create(EditorComponent* editor);

    // wi::gui::Window overrides
    void ResizeLayout() override;
    void Update(const wi::Canvas& canvas, float dt) override;
    void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;

    EditorComponent* editor = nullptr;

    // -------------------------------------------------------------------------
    // Toolbar / action buttons
    // -------------------------------------------------------------------------
    wi::gui::Button loadButton;
    wi::gui::Button saveButton;
    wi::gui::Button importLabelsButton;

    // -------------------------------------------------------------------------
    // Status feedback
    // -------------------------------------------------------------------------
    wi::gui::Label statusLabel;

    // -------------------------------------------------------------------------
    // Filter toggles (render-only — never passed to the tool)
    // -------------------------------------------------------------------------
    wi::gui::CheckBox zonesCheckBox;
    wi::gui::CheckBox citiesCheckBox;
    wi::gui::CheckBox labelsCheckBox;
    wi::gui::CheckBox biomesCheckBox;
    wi::gui::CheckBox stampsCheckBox;
    wi::gui::CheckBox clustersCheckBox;

    // -------------------------------------------------------------------------
    // Properties panel (shown when an anchor is selected)
    // -------------------------------------------------------------------------
    wi::gui::Label          propIdLabel;
    wi::gui::Label          propKindLabel;
    wi::gui::Label          propUVLabel;
    wi::gui::TextInputField propNameField;   // PlaceName only
    wi::gui::CheckBox       propVisibleBox;  // PlaceName only
    wi::gui::Slider         propConfSlider;  // PlaceName only
    wi::gui::TextInputField propAssetURIField;

private:
    // Owned data-layer tool — no GPU resources.
    // Declared as a pointer to avoid pulling in client headers in this header.
    std::unique_ptr<WickedTravelMapEditorTool> m_tool;

    bool        m_loaded = false;
    std::string m_layoutPath;

    // -------------------------------------------------------------------------
    // Selection and drag state
    // -------------------------------------------------------------------------
    std::string m_selectedAnchorId;
    bool        m_dragging       = false;
    XMFLOAT2    m_dragStartMouse = {};
    XMFLOAT2    m_dragStartUV    = {};   // stored as XMFLOAT2 for simplicity

    // -------------------------------------------------------------------------
    // Filter state (render-only)
    // -------------------------------------------------------------------------
    bool m_showZones   = true;
    bool m_showCities  = true;
    bool m_showLabels  = true;
    bool m_showBiomes  = true;
    bool m_showStamps  = true;
    bool m_showClusters = true;

    // -------------------------------------------------------------------------
    // Canvas geometry (computed in ResizeLayout)
    // -------------------------------------------------------------------------
    XMFLOAT2 m_canvasOrigin = {};
    XMFLOAT2 m_canvasSize   = {};

    // Map texture resource (lazy-loaded on first Render)
    mutable wi::Resource m_mapTexture;
    mutable bool         m_mapTextureLoaded = false;

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------
    XMFLOAT2 UVToCanvas(float u, float v) const;
    void     CanvasToUV(const XMFLOAT2& pos, float& outU, float& outV) const;
    std::string HitTest(const XMFLOAT2& clickPos) const;
    void     RefreshPropertiesPanel();
    void     ClearPropertiesPanel();
    void     SetStatus(const std::string& msg);
};
