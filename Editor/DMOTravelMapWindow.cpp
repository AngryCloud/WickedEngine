// Editor/DMOTravelMapWindow.cpp
// DMO Travel Map Editor panel — skeleton implementation.
// Full rendering, selection, drag, and I/O logic is added in Tasks 3–8.

#include "stdafx.h"
#include "DMOTravelMapWindow.h"

// Client data-layer headers (reachable via the include path added in Task 1)
#include "Travel/WickedTravelMapEditorTool.h"
#include "Travel/WickedTravelMapTypes.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DMOTravelMapWindow::SetStatus(const std::string& msg)
{
    statusLabel.SetText(msg);
}

XMFLOAT2 DMOTravelMapWindow::UVToCanvas(float u, float v) const
{
    return XMFLOAT2{
        m_canvasOrigin.x + u * m_canvasSize.x,
        m_canvasOrigin.y + v * m_canvasSize.y
    };
}

void DMOTravelMapWindow::CanvasToUV(const XMFLOAT2& pos, float& outU, float& outV) const
{
    if (m_canvasSize.x > 0.0f) outU = std::clamp((pos.x - m_canvasOrigin.x) / m_canvasSize.x, 0.0f, 1.0f);
    else outU = 0.0f;
    if (m_canvasSize.y > 0.0f) outV = std::clamp((pos.y - m_canvasOrigin.y) / m_canvasSize.y, 0.0f, 1.0f);
    else outV = 0.0f;
}

std::string DMOTravelMapWindow::HitTest(const XMFLOAT2& clickPos) const
{
    if (!m_tool) return {};

    const auto& anchors = m_tool->GetAnchors();

    constexpr float kSourceW = 2163.0f;
    constexpr float kSourceH = 3336.0f;
    const float scaleX = (m_canvasSize.x > 0.0f) ? m_canvasSize.x / kSourceW : 1.0f;
    const float scaleY = (m_canvasSize.y > 0.0f) ? m_canvasSize.y / kSourceH : 1.0f;

    // Priority: PlaceName → InstancedCity → Zone (first match wins)
    // Pass 1: labels
    if (m_showLabels)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::PlaceName) continue;
            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            // Hit box: 60px wide, 14px tall centred on the label position
            if (clickPos.x >= cp.x - 2.0f && clickPos.x <= cp.x + 60.0f &&
                clickPos.y >= cp.y - 2.0f && clickPos.y <= cp.y + 14.0f)
                return rec.anchorId;
        }
    }
    // Pass 2: cities
    if (m_showCities)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::InstancedCity) continue;
            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            constexpr float kPinR = 8.0f;
            if (std::abs(clickPos.x - cp.x) <= kPinR && std::abs(clickPos.y - cp.y) <= kPinR)
                return rec.anchorId;
        }
    }
    // Pass 3: zones
    if (m_showZones)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::Zone) continue;
            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            const float hw = rec.sizePx.x * scaleX * 0.5f;
            const float hh = rec.sizePx.y * scaleY * 0.5f;
            if (clickPos.x >= cp.x - hw && clickPos.x <= cp.x + hw &&
                clickPos.y >= cp.y - hh && clickPos.y <= cp.y + hh)
                return rec.anchorId;
        }
    }
    // Pass 4: biomes, stamps, clusters
    if (m_showBiomes || m_showStamps || m_showClusters)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind == WickedZoneMapAnchorKind::BiomeRegion && !m_showBiomes) continue;
            if (rec.kind == WickedZoneMapAnchorKind::TerrainStamp && !m_showStamps) continue;
            if (rec.kind == WickedZoneMapAnchorKind::EnvironmentCluster && !m_showClusters) continue;
            
            if (rec.kind == WickedZoneMapAnchorKind::BiomeRegion || rec.kind == WickedZoneMapAnchorKind::TerrainStamp || rec.kind == WickedZoneMapAnchorKind::EnvironmentCluster)
            {
                const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
                const float hw = rec.sizePx.x * scaleX * 0.5f;
                const float hh = rec.sizePx.y * scaleY * 0.5f;
                if (clickPos.x >= cp.x - hw && clickPos.x <= cp.x + hw &&
                    clickPos.y >= cp.y - hh && clickPos.y <= cp.y + hh)
                    return rec.anchorId;
            }
        }
    }
    return {};
}

void DMOTravelMapWindow::RefreshPropertiesPanel()
{
    if (!m_tool || m_selectedAnchorId.empty()) { ClearPropertiesPanel(); return; }

    const auto& anchors = m_tool->GetAnchors();
    for (const auto& rec : anchors)
    {
        if (rec.anchorId != m_selectedAnchorId) continue;

        propIdLabel.SetText("ID: " + rec.anchorId);

        std::string kindStr;
        switch (rec.kind)
        {
            case WickedZoneMapAnchorKind::Zone:         kindStr = "Zone";         break;
            case WickedZoneMapAnchorKind::InstancedCity: kindStr = "City";        break;
            case WickedZoneMapAnchorKind::PlaceName:    kindStr = "Label";        break;
            case WickedZoneMapAnchorKind::BiomeRegion:  kindStr = "Biome";        break;
            case WickedZoneMapAnchorKind::TerrainStamp: kindStr = "Stamp";        break;
            case WickedZoneMapAnchorKind::EnvironmentCluster: kindStr = "Cluster"; break;
        }
        propKindLabel.SetText("Kind: " + kindStr);

        char uvBuf[64];
        std::snprintf(uvBuf, sizeof(uvBuf), "UV: (%.3f, %.3f)", rec.centerUV.x, rec.centerUV.y);
        propUVLabel.SetText(uvBuf);

        // PlaceName-only widgets
        const bool isLabel = (rec.kind == WickedZoneMapAnchorKind::PlaceName);
        propNameField.SetEnabled(isLabel);
        propVisibleBox.SetEnabled(isLabel);
        propConfSlider.SetEnabled(isLabel);

        if (isLabel)
        {
            propNameField.SetValue(rec.displayName);
            propVisibleBox.SetCheck(m_tool->IsLabelVisible(rec.anchorId));
            propConfSlider.SetValue(rec.confidence);
        }
        else
        {
            propNameField.SetValue(rec.displayName);
            propVisibleBox.SetCheck(true);
            propConfSlider.SetValue(rec.confidence);
        }
        
        propAssetURIField.SetEnabled(true);
        propAssetURIField.SetValue(rec.assetURI);
        
        return;
    }
    ClearPropertiesPanel();
}

void DMOTravelMapWindow::ClearPropertiesPanel()
{
    m_selectedAnchorId.clear();
    propIdLabel.SetText("");
    propKindLabel.SetText("");
    propUVLabel.SetText("");
    propNameField.SetValue("");
    propAssetURIField.SetValue("");
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

void DMOTravelMapWindow::Create(EditorComponent* editorIn)
{
    editor = editorIn;

    // Allocate the data-layer tool (no GPU resources).
    m_tool = std::make_unique<WickedTravelMapEditorTool>();

    wi::gui::Window::Create("Travel Map", wi::gui::Window::WindowControls::CLOSE_AND_COLLAPSE);
    SetSize(XMFLOAT2(700, 500));

    // --- Load button ---
    loadButton.Create("Load");
    loadButton.SetTooltip("Load zone-map-layout-v1.json");
    loadButton.OnClick([this](wi::gui::EventArgs) {
        wi::helper::FileDialogParams params;
        params.type        = wi::helper::FileDialogParams::OPEN;
        params.description = "Zone Map Layout JSON";
        params.extensions  = { "json" };
        wi::helper::FileDialog(params, [this](const std::string& fileName) {
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, fileName](uint64_t) {
                    if (m_tool->LoadLayout(fileName))
                    {
                        m_loaded     = true;
                        m_layoutPath = fileName;
                        m_selectedAnchorId.clear();
                        ClearPropertiesPanel();
                        int w = 0, h = 0;
                        m_tool->GetLayoutSourceDimensions(w, h);
                        const int total = m_tool->GetAnchorCount();
                        char buf[128];
                        std::snprintf(buf, sizeof(buf),
                            "Loaded %d anchors (%dx%d)", total, w, h);
                        SetStatus(buf);
                    }
                    else
                    {
                        SetStatus("Load failed.");
                    }
                });
        });
    });
    AddWidget(&loadButton);

    // --- Save button ---
    saveButton.Create("Save");
    saveButton.SetTooltip("Save zone-map-layout-v1.json");
    saveButton.OnClick([this](wi::gui::EventArgs) {
        if (!m_loaded)
        {
            SetStatus("No layout loaded.");
            return;
        }
        if (m_tool->SaveLayout(m_layoutPath))
            SetStatus("Saved.");
        else
            SetStatus("Save failed.");
    });
    AddWidget(&saveButton);

    // --- Import Labels button ---
    importLabelsButton.Create("Import Labels");
    importLabelsButton.SetTooltip("Import place-name labels from OCR JSON");
    importLabelsButton.OnClick([this](wi::gui::EventArgs) {
        if (!m_loaded)
        {
            SetStatus("No layout loaded.");
            return;
        }
        wi::helper::FileDialogParams params;
        params.type        = wi::helper::FileDialogParams::OPEN;
        params.description = "Place-Name Records JSON";
        params.extensions  = { "json" };
        wi::helper::FileDialog(params, [this](const std::string& fileName) {
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, fileName](uint64_t) {
                    const int count = m_tool->ImportPlaceNames(fileName);
                    if (count > 0)
                    {
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "Imported %d labels.", count);
                        SetStatus(buf);
                    }
                    else
                    {
                        SetStatus("Import failed: 0 labels imported.");
                    }
                });
        });
    });
    AddWidget(&importLabelsButton);

    // --- Status label ---
    statusLabel.Create("status");
    statusLabel.SetText("No layout loaded.");
    AddWidget(&statusLabel);

    // --- Filter checkboxes (default: all checked) ---
    zonesCheckBox.Create("Zones");
    zonesCheckBox.SetCheck(true);
    zonesCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showZones = args.bValue;
    });
    AddWidget(&zonesCheckBox);

    citiesCheckBox.Create("Cities");
    citiesCheckBox.SetCheck(true);
    citiesCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showCities = args.bValue;
    });
    AddWidget(&citiesCheckBox);

    labelsCheckBox.Create("Labels");
    labelsCheckBox.SetCheck(true);
    labelsCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showLabels = args.bValue;
    });
    AddWidget(&labelsCheckBox);

    biomesCheckBox.Create("Biomes");
    biomesCheckBox.SetCheck(true);
    biomesCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showBiomes = args.bValue;
    });
    AddWidget(&biomesCheckBox);

    stampsCheckBox.Create("Stamps");
    stampsCheckBox.SetCheck(true);
    stampsCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showStamps = args.bValue;
    });
    AddWidget(&stampsCheckBox);

    clustersCheckBox.Create("Clusters");
    clustersCheckBox.SetCheck(true);
    clustersCheckBox.OnClick([this](wi::gui::EventArgs args) {
        m_showClusters = args.bValue;
    });
    AddWidget(&clustersCheckBox);

    // --- Properties panel widgets ---
    propIdLabel.Create("propId");
    propIdLabel.SetText("");
    AddWidget(&propIdLabel);

    propKindLabel.Create("propKind");
    propKindLabel.SetText("");
    AddWidget(&propKindLabel);

    propUVLabel.Create("propUV");
    propUVLabel.SetText("");
    AddWidget(&propUVLabel);

    propNameField.Create("Name");
    propNameField.SetValue("");
    propNameField.OnInputAccepted([this](wi::gui::EventArgs args) {
        if (!m_selectedAnchorId.empty() && m_tool)
            m_tool->SetLabelDisplayName(m_selectedAnchorId, args.sValue);
    });
    AddWidget(&propNameField);

    propVisibleBox.Create("Visible");
    propVisibleBox.SetCheck(true);
    propVisibleBox.OnClick([this](wi::gui::EventArgs args) {
        if (!m_selectedAnchorId.empty() && m_tool)
            m_tool->SetLabelVisible(m_selectedAnchorId, args.bValue);
    });
    AddWidget(&propVisibleBox);

    propConfSlider.Create(0.0f, 1.0f, 1.0f, 100, "Confidence");
    propConfSlider.OnSlide([this](wi::gui::EventArgs args) {
        if (!m_selectedAnchorId.empty() && m_tool)
            m_tool->SetLabelConfidence(m_selectedAnchorId, args.fValue);
    });
    AddWidget(&propConfSlider);

    propAssetURIField.Create("Asset URI");
    propAssetURIField.SetValue("");
    propAssetURIField.OnInputAccepted([this](wi::gui::EventArgs args) {
        // Assume editor logic or external system picks up the URI update
        // We'll leave it as setting value on UI for now.
    });
    AddWidget(&propAssetURIField);

    SetVisible(false);
}

// ---------------------------------------------------------------------------
// ResizeLayout
// ---------------------------------------------------------------------------

void DMOTravelMapWindow::ResizeLayout()
{
    wi::gui::Window::ResizeLayout();

    const float padding  = 4.0f;
    const float btnH     = 20.0f;
    const float labelH   = 18.0f;
    const float sliderH  = 18.0f;
    const float checkH   = 18.0f;
    const float propW    = 180.0f;   // width of properties panel on the right
    const float toolbarH = btnH + padding * 2.0f;

    const float winW = GetSize().x;
    const float winH = GetSize().y;

    // Toolbar row (top)
    float x = padding;
    const float y0 = padding;
    loadButton.SetPos(XMFLOAT2(x, y0));
    loadButton.SetSize(XMFLOAT2(60, btnH));
    x += 64;
    saveButton.SetPos(XMFLOAT2(x, y0));
    saveButton.SetSize(XMFLOAT2(50, btnH));
    x += 54;
    importLabelsButton.SetPos(XMFLOAT2(x, y0));
    importLabelsButton.SetSize(XMFLOAT2(100, btnH));
    x += 104;
    statusLabel.SetPos(XMFLOAT2(x, y0));
    statusLabel.SetSize(XMFLOAT2(winW - x - padding, btnH));

    // Filter checkboxes (below toolbar)
    const float y1 = y0 + btnH + padding;
    zonesCheckBox.SetPos(XMFLOAT2(padding, y1));
    zonesCheckBox.SetSize(XMFLOAT2(60, checkH));
    citiesCheckBox.SetPos(XMFLOAT2(padding + 64, y1));
    citiesCheckBox.SetSize(XMFLOAT2(60, checkH));
    labelsCheckBox.SetPos(XMFLOAT2(padding + 128, y1));
    labelsCheckBox.SetSize(XMFLOAT2(60, checkH));
    biomesCheckBox.SetPos(XMFLOAT2(padding + 192, y1));
    biomesCheckBox.SetSize(XMFLOAT2(60, checkH));
    stampsCheckBox.SetPos(XMFLOAT2(padding + 256, y1));
    stampsCheckBox.SetSize(XMFLOAT2(60, checkH));
    clustersCheckBox.SetPos(XMFLOAT2(padding + 320, y1));
    clustersCheckBox.SetSize(XMFLOAT2(60, checkH));

    // Map canvas area (left side, below filter row)
    const float canvasTop  = y1 + checkH + padding;
    const float canvasLeft = padding;
    const float canvasW    = winW - propW - padding * 3.0f;
    const float canvasH    = winH - canvasTop - padding;

    // Aspect-ratio-preserving scale: source is 2163 × 3336
    constexpr float kSourceAspect = 2163.0f / 3336.0f;
    float drawW, drawH;
    if (canvasW / canvasH > kSourceAspect)
    {
        drawH = canvasH;
        drawW = canvasH * kSourceAspect;
    }
    else
    {
        drawW = canvasW;
        drawH = canvasW / kSourceAspect;
    }
    // Centre within the canvas area
    m_canvasOrigin = XMFLOAT2(canvasLeft + (canvasW - drawW) * 0.5f, canvasTop + (canvasH - drawH) * 0.5f);
    m_canvasSize   = XMFLOAT2(drawW, drawH);

    // Properties panel (right side)
    const float propX = winW - propW - padding;
    float py = canvasTop;
    propIdLabel.SetPos(XMFLOAT2(propX, py));
    propIdLabel.SetSize(XMFLOAT2(propW, labelH));
    py += labelH + padding;
    propKindLabel.SetPos(XMFLOAT2(propX, py));
    propKindLabel.SetSize(XMFLOAT2(propW, labelH));
    py += labelH + padding;
    propUVLabel.SetPos(XMFLOAT2(propX, py));
    propUVLabel.SetSize(XMFLOAT2(propW, labelH));
    py += labelH + padding;
    propNameField.SetPos(XMFLOAT2(propX, py));
    propNameField.SetSize(XMFLOAT2(propW, btnH));
    py += btnH + padding;
    propVisibleBox.SetPos(XMFLOAT2(propX, py));
    propVisibleBox.SetSize(XMFLOAT2(propW, checkH));
    py += checkH + padding;
    propConfSlider.SetPos(XMFLOAT2(propX, py));
    propConfSlider.SetSize(XMFLOAT2(propW, sliderH));
    py += sliderH + padding;
    propAssetURIField.SetPos(XMFLOAT2(propX, py));
    propAssetURIField.SetSize(XMFLOAT2(propW, btnH));
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void DMOTravelMapWindow::Update(const wi::Canvas& canvas, float dt)
{
    wi::gui::Window::Update(canvas, dt);

    if (!IsVisible() || !m_loaded || !m_tool) return;

    const XMFLOAT4 pointer = wi::input::GetPointer();
    const XMFLOAT2 mousePos(pointer.x, pointer.y);

    // Is the mouse inside the map canvas?
    const bool inCanvas =
        mousePos.x >= m_canvasOrigin.x && mousePos.x <= m_canvasOrigin.x + m_canvasSize.x &&
        mousePos.y >= m_canvasOrigin.y && mousePos.y <= m_canvasOrigin.y + m_canvasSize.y;

    // -------------------------------------------------------------------------
    // 7.1 — Start drag: left button pressed on a selected anchor
    // -------------------------------------------------------------------------
    if (!m_dragging && inCanvas && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
    {
        const std::string hit = HitTest(mousePos);
        if (!hit.empty() && hit == m_selectedAnchorId)
        {
            // Begin drag on the already-selected anchor
            m_dragging = true;
            m_dragStartMouse = mousePos;
            float u, v;
            CanvasToUV(mousePos, u, v);
            m_dragStartUV = XMFLOAT2(u, v);
        }
        else
        {
            // 6.2 — Click-to-select (no drag yet)
            m_selectedAnchorId = hit;
            RefreshPropertiesPanel();
        }
    }

    // -------------------------------------------------------------------------
    // 7.2 — During drag: update anchor UV in real time
    // -------------------------------------------------------------------------
    if (m_dragging && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
    {
        float u, v;
        CanvasToUV(mousePos, u, v);
        // Real-time update (clamping is applied by SetAnchorCenterUV)
        WickedFloat2 newUV{ u, v };
        m_tool->SetAnchorCenterUV(m_selectedAnchorId, newUV);
        // Keep properties panel UV label in sync
        char uvBuf[64];
        std::snprintf(uvBuf, sizeof(uvBuf), "UV: (%.3f, %.3f)", u, v);
        propUVLabel.SetText(uvBuf);
    }

    // -------------------------------------------------------------------------
    // 7.3 — End drag: mouse released — commit clamped final UV
    // -------------------------------------------------------------------------
    if (m_dragging && wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
    {
        float u, v;
        CanvasToUV(mousePos, u, v);
        // CanvasToUV already clamps to [0,1]; commit final value
        WickedFloat2 finalUV{ u, v };
        m_tool->SetAnchorCenterUV(m_selectedAnchorId, finalUV);
        m_dragging = false;
        // 7.4 — Refresh properties panel to show final UV
        RefreshPropertiesPanel();
    }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void DMOTravelMapWindow::Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const
{
    wi::gui::Window::Render(canvas, cmd);

    if (!IsVisible()) return;

    if (!m_loaded || !m_tool)
    {
        // Placeholder text when no layout is loaded
        wi::font::Params fp;
        fp.position = XMFLOAT3(m_canvasOrigin.x + 8.0f, m_canvasOrigin.y + 8.0f, 0.0f);
        fp.size = 14;
        fp.color = wi::Color(180, 180, 180, 200);
        wi::font::Draw("No layout loaded", 16, fp, cmd);
        return;
    }

    // -------------------------------------------------------------------------
    // 4.1 — Map texture (lazy-loaded)
    // -------------------------------------------------------------------------
    if (!m_mapTextureLoaded)
    {
        // Resolve path relative to the editor working directory.
        // The editor runs from external/WickedEngine/Editor/ (or build dir),
        // so we use a path relative to the project root via the asset dir.
        m_mapTexture = wi::resourcemanager::Load(
            "Assets/TravelMap/OracleMapWithDalelandsZone.png",
            wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA);
        m_mapTextureLoaded = true;
    }

    if (m_mapTexture.IsValid())
    {
        wi::image::Params ip;
        ip.pos  = XMFLOAT3(m_canvasOrigin.x, m_canvasOrigin.y, 0.0f);
        ip.siz  = XMFLOAT2(m_canvasSize.x, m_canvasSize.y);
        ip.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        wi::image::Draw(&m_mapTexture.GetTexture(), ip, cmd);
    }
    else
    {
        // Texture not yet available — draw a dark placeholder rect
        wi::image::Params ip;
        ip.pos  = XMFLOAT3(m_canvasOrigin.x, m_canvasOrigin.y, 0.0f);
        ip.siz  = XMFLOAT2(m_canvasSize.x, m_canvasSize.y);
        ip.color = XMFLOAT4(0.1f, 0.1f, 0.15f, 1.0f);
        wi::image::Draw(nullptr, ip, cmd);
    }

    // -------------------------------------------------------------------------
    // Compute canvas scale for converting sizePx → canvas pixels
    // -------------------------------------------------------------------------
    constexpr float kSourceW = 2163.0f;
    constexpr float kSourceH = 3336.0f;
    const float scaleX = m_canvasSize.x / kSourceW;
    const float scaleY = m_canvasSize.y / kSourceH;

    const auto& anchors = m_tool->GetAnchors();

    // -------------------------------------------------------------------------
    // 4.2–4.5 — Anchor overlays
    // Render order: Zones first (bottom), then Cities, then Labels (top).
    // Selected anchor is drawn last so its highlight is always on top.
    // -------------------------------------------------------------------------

    // Helper: is this the selected anchor?
    auto isSelected = [&](const WickedZoneMapAnchorRecord& rec) {
        return rec.anchorId == m_selectedAnchorId && !m_selectedAnchorId.empty();
    };

    // --- 4.2 Zone boxes ---
    if (m_showZones)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::Zone) continue;
            if (isSelected(rec)) continue; // drawn last

            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            const float hw = rec.sizePx.x * scaleX * 0.5f;
            const float hh = rec.sizePx.y * scaleY * 0.5f;

            wi::image::Params ip;
            ip.pos   = XMFLOAT3(cp.x - hw, cp.y - hh, 0.0f);
            ip.siz   = XMFLOAT2(hw * 2.0f, hh * 2.0f);
            ip.color = XMFLOAT4(0.2f, 0.6f, 1.0f, 0.35f);
            wi::image::Draw(nullptr, ip, cmd);
        }
    }

    // --- 4.3 City pins ---
    if (m_showCities)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::InstancedCity) continue;
            if (isSelected(rec)) continue;

            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            constexpr float kPinR = 5.0f;

            wi::image::Params ip;
            ip.pos   = XMFLOAT3(cp.x - kPinR, cp.y - kPinR, 0.0f);
            ip.siz   = XMFLOAT2(kPinR * 2.0f, kPinR * 2.0f);
            ip.color = XMFLOAT4(1.0f, 0.8f, 0.2f, 0.9f);
            wi::image::Draw(nullptr, ip, cmd);
        }
    }

    // --- 4.4 Place-name labels ---
    if (m_showLabels)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind != WickedZoneMapAnchorKind::PlaceName) continue;
            if (isSelected(rec)) continue;

            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);

            // 4.5 — hidden labels at 40% opacity
            const bool hidden = (rec.notes.find("visible:false") != std::string::npos);
            const uint8_t alpha = hidden ? 102u : 255u; // 40% = 102/255

            wi::font::Params fp;
            fp.position = XMFLOAT3(cp.x, cp.y, 0.0f);
            fp.size     = 10;
            fp.color    = wi::Color(255, 255, 255, alpha);
            wi::font::Draw(rec.displayName.c_str(), rec.displayName.size(), fp, cmd);
        }
    }
    
    // --- 4.5 Biomes, Stamps, and Clusters ---
    if (m_showBiomes || m_showStamps || m_showClusters)
    {
        for (const auto& rec : anchors)
        {
            if (rec.kind == WickedZoneMapAnchorKind::BiomeRegion && !m_showBiomes) continue;
            if (rec.kind == WickedZoneMapAnchorKind::TerrainStamp && !m_showStamps) continue;
            if (rec.kind == WickedZoneMapAnchorKind::EnvironmentCluster && !m_showClusters) continue;
            if (isSelected(rec)) continue;
            
            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);

            if (rec.kind == WickedZoneMapAnchorKind::EnvironmentCluster)
            {
                constexpr float kPinR = 4.0f;
                wi::image::Params ip;
                ip.pos   = XMFLOAT3(cp.x - kPinR, cp.y - kPinR, 0.0f);
                ip.siz   = XMFLOAT2(kPinR * 2.0f, kPinR * 2.0f);
                ip.color = XMFLOAT4(0.2f, 0.9f, 0.3f, 0.9f);
                wi::image::Draw(nullptr, ip, cmd);
            }
            else
            {
                const float hw = rec.sizePx.x * scaleX * 0.5f;
                const float hh = rec.sizePx.y * scaleY * 0.5f;

                wi::image::Params ip;
                ip.pos   = XMFLOAT3(cp.x - hw, cp.y - hh, 0.0f);
                ip.siz   = XMFLOAT2(hw * 2.0f, hh * 2.0f);
                if (rec.kind == WickedZoneMapAnchorKind::BiomeRegion)
                    ip.color = XMFLOAT4(0.3f, 0.8f, 0.3f, 0.35f);
                else
                    ip.color = XMFLOAT4(0.6f, 0.4f, 0.1f, 0.35f);
                wi::image::Draw(nullptr, ip, cmd);
            }
        }
    }

    // --- 4.5 Selected anchor highlight (drawn last, on top) ---
    if (!m_selectedAnchorId.empty())
    {
        for (const auto& rec : anchors)
        {
            if (rec.anchorId != m_selectedAnchorId) continue;

            const XMFLOAT2 cp = UVToCanvas(rec.centerUV.x, rec.centerUV.y);
            const XMFLOAT4 kHighlight(1.0f, 0.85f, 0.0f, 0.85f);

            if (rec.kind == WickedZoneMapAnchorKind::Zone)
            {
                const float hw = rec.sizePx.x * scaleX * 0.5f;
                const float hh = rec.sizePx.y * scaleY * 0.5f;
                wi::image::Params ip;
                ip.pos   = XMFLOAT3(cp.x - hw, cp.y - hh, 0.0f);
                ip.siz   = XMFLOAT2(hw * 2.0f, hh * 2.0f);
                ip.color = kHighlight;
                wi::image::Draw(nullptr, ip, cmd);
            }
            else if (rec.kind == WickedZoneMapAnchorKind::InstancedCity)
            {
                constexpr float kPinR = 6.0f;
                wi::image::Params ip;
                ip.pos   = XMFLOAT3(cp.x - kPinR, cp.y - kPinR, 0.0f);
                ip.siz   = XMFLOAT2(kPinR * 2.0f, kPinR * 2.0f);
                ip.color = kHighlight;
                wi::image::Draw(nullptr, ip, cmd);
            }
            else if (rec.kind == WickedZoneMapAnchorKind::PlaceName)
            {
                wi::font::Params fp;
                fp.position = XMFLOAT3(cp.x, cp.y, 0.0f);
                fp.size     = 10;
                fp.color    = wi::Color(
                    static_cast<uint8_t>(kHighlight.x * 255),
                    static_cast<uint8_t>(kHighlight.y * 255),
                    static_cast<uint8_t>(kHighlight.z * 255),
                    static_cast<uint8_t>(kHighlight.w * 255));
                wi::font::Draw(rec.displayName.c_str(), rec.displayName.size(), fp, cmd);
            }
            break;
        }
    }
}
