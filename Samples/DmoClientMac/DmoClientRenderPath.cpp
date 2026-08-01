// external/WickedEngine/Samples/DmoClientMac/DmoClientRenderPath.cpp
// WICKED-UI-03A-07B — concrete wi::gui visual sink + DMO front-door RenderPath.
//
// This is the engine-linked half of the projector. It includes the real
// WickedEngine and the ENGINE-NEUTRAL POD headers only (WickedUIVisualSink.h,
// WickedFrontDoorHost.h) — never the shim or the WickedUIWidget/theme headers —
// so there is no XMFLOAT2 collision. The shim-built dmo_wicked_ui_min lib holds
// the widget tree + theme + projector and calls back through the POD sink.

#include "DmoClientRenderPath.h"

#include "WickedEngine.h"

#include "UI/Projection/WickedUIVisualSink.h"
#include "UI/Projection/WickedFrontDoorHost.h"
#include "Application/WickedGodClientRuntimeAccess.h"
#include "GodClient/WickedGodClientEditorBridgeTypes.h"
#include "wiGUI.h"

#include <cstdio>
#include <memory>
#include <vector>
#include <unordered_map>

namespace {

// Frozen WickedUIWidgetType ordinals we special-case (see WickedUIWidgetTypes.h).
enum : int {
    kTypePage = 0,
    kTypeText = 1,
    kTypeButton = 2,
    kTypeComposite = 6,
    kTypeList = 7,
    kTypeTable = 8,
    kTypeTreeView = 12,
    kTypeNinePatchFrame = 17,
};

bool IsStructuralContainer(const WickedUI::WickedUIWidgetVisual& v) noexcept
{
    switch (v.typeOrdinal)
    {
    case kTypePage:
    case kTypeComposite:
    case kTypeList:
    case kTypeTable:
    case kTypeTreeView:
    case kTypeNinePatchFrame:
        return true;
    default:
        return false;
    }
}

bool IsEffectivelyTransparent(const WickedUI::WickedUIWidgetVisual& v) noexcept
{
    return (v.colorA * v.alpha) <= 0.001f;
}

bool HasRenderableContent(const WickedUI::WickedUIWidgetVisual& v) noexcept
{
    return (v.text != nullptr && v.text[0] != '\0') ||
           (v.imagePath != nullptr && v.imagePath[0] != '\0');
}

bool ShouldElideVisual(const WickedUI::WickedUIWidgetVisual& v) noexcept
{
    // The portable host uses these nodes as layout containers only. Materializing
    // them into top-level wi::gui widgets creates giant invisible hitboxes that can
    // steal focus and destabilize the live Mac sample when users click outside the
    // visible UI chrome. Skip them entirely unless they actually draw something.
    return IsStructuralContainer(v) && IsEffectivelyTransparent(v) && !HasRenderableContent(v);
}

// Materializes each projected visual as a wi::gui widget and owns their lifetime
// (the GUI only borrows the raw pointers).
class WiGuiSink final : public WickedUI::IWickedUIWidgetVisualSink
{
public:
    wi::gui::GUI* gui = nullptr;
    std::vector<std::unique_ptr<wi::gui::Widget>> owned;

    class PassiveLabel final : public wi::gui::Label
    {
    public:
        void Update(const wi::Canvas& canvas, float dt) override
        {
            // Render-only projection surface: keep Wicked's transform/font/sprite
            // bookkeeping, but deliberately skip Label's interactive logic so large
            // backdrop widgets never participate in focus, click, scroll, or z-order
            // churn.
            Widget::Update(canvas, dt);

            switch (font.params.h_align)
            {
            case wi::font::WIFALIGN_LEFT:
                font.params.posX = translation.x + 2.0f;
                break;
            case wi::font::WIFALIGN_RIGHT:
                font.params.posX = translation.x + scale.x - 2.0f;
                break;
            case wi::font::WIFALIGN_CENTER:
            default:
                font.params.posX = translation.x + scale.x * 0.5f;
                break;
            }

            switch (font.params.v_align)
            {
            case wi::font::WIFALIGN_TOP:
                font.params.posY = translation.y + 2.0f;
                break;
            case wi::font::WIFALIGN_BOTTOM:
                font.params.posY = translation.y + scale.y - 2.0f;
                break;
            case wi::font::WIFALIGN_CENTER:
            default:
                font.params.posY = translation.y + scale.y * 0.5f;
                break;
            }

            if (font.params.h_align != wi::font::WIFALIGN_CENTER)
                font.params.h_wrap = scale.x - 4.0f;
            else
                font.params.h_wrap = -1.0f;
        }
    };

    // Push a resolved visual's text/rect/color onto a widget (shared by create +
    // in-place update). The projector emits ONE color per widget; wi::gui splits it
    // into a background fill (SetColor) and a text color (font.params):
    //   - Text  -> the color is the FONT color; background stays transparent so the
    //              panel behind shows through (dark panel + light text).
    //   - Button/panel/field -> the color fills the background; buttons get a light
    //              label so it reads against the dark surface.
    static void ApplyVisual(wi::gui::Widget& w, const WickedUI::WickedUIWidgetVisual& v)
    {
        const char* text = v.text ? v.text : "";
        w.SetText(v.typeOrdinal == kTypeButton || v.typeOrdinal == kTypeText ? text : "");
        w.SetPos(XMFLOAT2(v.x, v.y));
        w.SetSize(XMFLOAT2(v.width, v.height));

        if (v.typeOrdinal == kTypeButton)
        {
            w.font.params.h_align = wi::font::WIFALIGN_CENTER;
            w.font.params.v_align = wi::font::WIFALIGN_CENTER;
        }
        else
        {
            w.font.params.h_align = wi::font::WIFALIGN_LEFT;
            w.font.params.v_align = wi::font::WIFALIGN_TOP;
        }

        const wi::Color themed =
            wi::Color::fromFloat4(XMFLOAT4(v.colorR, v.colorG, v.colorB, v.colorA * v.alpha));

        // Image-backed widget: load the texture and apply it as the widget's sprite.
        // The resource manager caches by name (repeat Loads return the same handle,
        // no re-decode), and SetImage holds a reference so the texture stays alive.
        // A textured widget renders the image un-tinted (white * node alpha); if the
        // texture fails to load we fall through to the flat colored fill below.
        const bool wantsImage = v.imagePath != nullptr && v.imagePath[0] != '\0';
        wi::Resource img;
        if (wantsImage)
            img = wi::resourcemanager::Load(v.imagePath);

        if (wantsImage && img.IsValid())
        {
            w.SetImage(img);
            w.SetColor(wi::Color::fromFloat4(XMFLOAT4(1.0f, 1.0f, 1.0f, v.alpha)));
            if (v.typeOrdinal == kTypeButton || v.typeOrdinal == kTypeText)
                w.font.params.color = wi::Color::fromFloat4(XMFLOAT4(0.92f, 0.92f, 0.95f, 1.0f));
        }
        else if (v.typeOrdinal == kTypeText)
        {
            w.SetColor(wi::Color::fromFloat4(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)));
            w.font.params.color = themed;
        }
        else
        {
            w.SetColor(themed);
            if (v.typeOrdinal == kTypeButton)
                w.font.params.color = wi::Color::fromFloat4(XMFLOAT4(0.92f, 0.92f, 0.95f, 1.0f));
        }
    }

    void* CreateVisual(const WickedUI::WickedUIWidgetVisual& v, void* parentHandle) override
    {
        if (ShouldElideVisual(v))
            return nullptr;

        // The projector already resolves absolute coordinates, so every widget is
        // added flat to the GUI at its absolute position (no wi parenting needed).
        std::unique_ptr<wi::gui::Widget> widget;
        const char* name = v.name ? v.name : "";
        // The portable host owns all interaction semantics and hit-testing. The
        // engine-linked sink is strictly a render surface, so use labels for every
        // projected node instead of live wi::gui buttons. This avoids a second,
        // conflicting button state machine mutating visuals when the user clicks.
        auto l = std::make_unique<PassiveLabel>();
        l->Create(name);
        widget = std::move(l);
        ApplyVisual(*widget, v);

        // NOTE: do NOT AddWidget here. wi::gui z-orders first-added as frontmost, but the
        // projector emits parent(backdrop)-before-children, so we defer and add reversed
        // (see DmoFrontDoorPath::Reproject) — else the backdrop paints over every child.
        wi::gui::Widget* raw = widget.get();
        owned.push_back(std::move(widget));
        return raw;
    }

    // In-place update of an already-created widget (no add/remove). The value-update
    // path uses this so a widget the user is pressing is never torn down mid-frame.
    void UpdateVisual(void* handle, const WickedUI::WickedUIWidgetVisual& v) override
    {
        if (handle != nullptr)
            ApplyVisual(*static_cast<wi::gui::Widget*>(handle), v);
    }

    // Detach + free all widgets so the tree can be re-projected (controller state
    // changed, e.g. a status line update after a sign-in click).
    void Clear()
    {
        if (gui != nullptr)
            for (auto& w : owned)
                gui->RemoveWidget(w.get());
        owned.clear();
    }
};

class DmoGodClientPanelWindowFactory final : public dmo::wicked::IWickedGodClientPanelWindowFactory
{
public:
    explicit DmoGodClientPanelWindowFactory(wi::gui::GUI& gui) noexcept
        : m_gui(gui)
    {
    }

    bool CreatePanelWindow(const dmo::wicked::GodClientEditorPanelKind kind,
                           const std::uint64_t panelInstanceId) override
    {
        if (m_windows.find(panelInstanceId) != m_windows.end())
            return true;

        auto window = std::make_unique<wi::gui::Window>();
        window->Create(PanelTitle(kind), wi::gui::Window::WindowControls::CLOSE);
        window->SetName("godclient.panel." + std::to_string(panelInstanceId));
        const XMFLOAT4 layout = ResolvePanelLayout(kind, static_cast<std::uint32_t>(m_windows.size()));
        window->SetSize(XMFLOAT2(layout.z, layout.w));
        window->SetPos(XMFLOAT2(layout.x, layout.y));
        window->SetColor(wi::Color(44, 44, 56, 235));

        auto body = std::make_unique<wi::gui::Label>();
        body->SetName(window->GetName() + ".body");
        body->SetPos(XMFLOAT2(12.0f, 36.0f));
        body->SetSize(XMFLOAT2(layout.z - 24.0f, layout.w - 80.0f));
        body->SetText(PanelBody(kind));
        body->SetColor(wi::Color(228, 228, 232, 255));
        window->AddWidget(body.get(), wi::gui::Window::AttachmentOptions::NONE);

        m_gui.AddWidget(window.get());
        OwnedWindow owned;
        owned.window = std::move(window);
        owned.body = std::move(body);
        m_windows.emplace(panelInstanceId, std::move(owned));
        return true;
    }

    void DestroyPanelWindow(const std::uint64_t panelInstanceId) override
    {
        const auto it = m_windows.find(panelInstanceId);
        if (it == m_windows.end())
            return;
        m_gui.RemoveWidget(it->second.window.get());
        m_windows.erase(it);
    }

private:
    struct OwnedWindow
    {
        std::unique_ptr<wi::gui::Window> window;
        std::unique_ptr<wi::gui::Label> body;
    };

    static const char* PanelTitle(const dmo::wicked::GodClientEditorPanelKind kind) noexcept
    {
        using dmo::wicked::GodClientEditorPanelKind;
        switch (kind)
        {
        case GodClientEditorPanelKind::AssetPlacer: return "Asset Placer";
        case GodClientEditorPanelKind::WorldEditorHub: return "World Builder Hub";
        case GodClientEditorPanelKind::TravelMap: return "Travel Map";
        case GodClientEditorPanelKind::GaussianSplat: return "Gaussian Splat";
        case GodClientEditorPanelKind::Terrain: return "Terrain Tools";
        case GodClientEditorPanelKind::Custom: break;
        }
        return "Godclient Panel";
    }

    static std::string PanelBody(const dmo::wicked::GodClientEditorPanelKind kind)
    {
        using dmo::wicked::GodClientEditorPanelKind;
        switch (kind)
        {
        case GodClientEditorPanelKind::AssetPlacer:
            return "Recovered editor shell for asset placement. Full placement controls can be adapted into this visible window incrementally.";
        case GodClientEditorPanelKind::WorldEditorHub:
            return "Recovered world-builder hub shell. Use this as the bounded in-client editor frame while the player HUD remains active underneath.";
        case GodClientEditorPanelKind::TravelMap:
            return "Recovered travel-map editor shell. This window is mounted on the live GUI path rather than opening headless.";
        case GodClientEditorPanelKind::GaussianSplat:
            return "Recovered Gaussian splat editing shell. Rendering/editor controls can be moved here panel by panel.";
        case GodClientEditorPanelKind::Terrain:
            return "Recovered terrain tooling shell. This is the visible anchor for terrain, spline, and world-shaping tools.";
        case GodClientEditorPanelKind::Custom:
            break;
        }
        return "Recovered Godclient panel shell.";
    }

    static XMFLOAT4 ResolvePanelLayout(
        const dmo::wicked::GodClientEditorPanelKind kind,
        const std::uint32_t fallbackIndex) noexcept
    {
        const float logicalWidth = WickedUI::GetFrontDoorLogicalWidth();
        const float logicalHeight = WickedUI::GetFrontDoorLogicalHeight();
        const float margin = 24.0f;
        const float rightColumnX = logicalWidth - 340.0f - margin;
        const float leftColumnX = margin;
        const float midLeftX = margin + 320.0f;
        switch (kind)
        {
        case dmo::wicked::GodClientEditorPanelKind::WorldEditorHub:
            return XMFLOAT4(leftColumnX, margin, 300.0f, 220.0f);
        case dmo::wicked::GodClientEditorPanelKind::AssetPlacer:
            return XMFLOAT4(leftColumnX, logicalHeight - 250.0f, 300.0f, 220.0f);
        case dmo::wicked::GodClientEditorPanelKind::TravelMap:
            return XMFLOAT4(rightColumnX, margin, 340.0f, 240.0f);
        case dmo::wicked::GodClientEditorPanelKind::GaussianSplat:
            return XMFLOAT4(rightColumnX, 276.0f, 340.0f, 220.0f);
        case dmo::wicked::GodClientEditorPanelKind::Terrain:
            return XMFLOAT4(rightColumnX, logicalHeight - 250.0f, 340.0f, 220.0f);
        case dmo::wicked::GodClientEditorPanelKind::Custom:
        default:
            break;
        }

        const float x = (fallbackIndex % 2u) == 0u ? midLeftX : rightColumnX;
        const float y = margin + static_cast<float>(fallbackIndex / 2u) * 244.0f;
        return XMFLOAT4(x, y, 320.0f, 220.0f);
    }

    wi::gui::GUI& m_gui;
    std::unordered_map<std::uint64_t, OwnedWindow> m_windows;
};

class DmoFrontDoorPath final : public wi::RenderPath2D
{
public:
    void Load() override
    {
        wi::RenderPath2D::Load();
        WickedUI::SetFrontDoorLogicalSize(GetLogicalWidth(), GetLogicalHeight());
        m_sink.gui = &GetGUI();
        if (!m_godClientPanelFactory)
            m_godClientPanelFactory = std::make_unique<DmoGodClientPanelWindowFactory>(GetGUI());
        (void)InstallGodClientPanelWindowFactory(m_godClientPanelFactory.get());
        const std::size_t n = Reproject();
        m_lastStructureToken = WickedUI::FrontDoorStructureToken();
        m_lastValueToken = WickedUI::FrontDoorStateToken();
        std::fprintf(stderr, "[DmoClient] front door projected: %zu widgets\n", n);
    }

    void Update(float dt) override
    {
        const bool mousePressed = wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
        const bool mouseReleased = wi::input::Release(wi::input::MOUSE_BUTTON_LEFT);
        const bool mouseHeld = wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        const bool secondaryPressed = wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT);
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool pointerMoved =
            !m_hasLastPointer ||
            pointer.x != m_lastPointerX ||
            pointer.y != m_lastPointerY;

        // STRUCTURAL rebuild (screens mounted/unmounted) is the only path that tears
        // down + recreates wi::gui widgets. Do it at the top of the frame, and NEVER
        // while the mouse is in a press/release transition — freeing a widget while
        // Wicked GUI still has per-frame interaction bookkeeping for it corrupts the
        // GUI (this was the "UI disappears on click" failure). We require one fully
        // idle frame after the transition before rebuilding. Value changes
        // (vitals/chat/status) go through the in-place path below instead, so they
        // never destroy a widget on the interaction frame.
        if (m_reprojectPending)
        {
            if (mouseHeld || mousePressed || mouseReleased)
            {
                m_waitForIdleMouseFrame = true;
            }
            else if (m_waitForIdleMouseFrame)
            {
                m_waitForIdleMouseFrame = false;
            }
            else
            {
                const std::size_t n = Reproject();
                std::fprintf(stderr, "[DmoClient] reprojected: %zu widgets\n", n);
                m_reprojectPending = false;
                m_lastStructureToken = WickedUI::FrontDoorStructureToken();
                m_lastValueToken = WickedUI::FrontDoorStateToken(); // fresh tree = current values
            }
        }

        wi::RenderPath2D::Update(dt);
        WickedUI::SetFrontDoorLogicalSize(GetLogicalWidth(), GetLogicalHeight());

        // Advance controllers (Refresh/animation — e.g. the loading bar).
        WickedUI::TickFrontDoor(dt);
        // Keep the engine-linked sink visually authoritative every frame. The
        // portable host owns the real widget tree, so re-applying the current POD
        // visuals is cheap and prevents any local wi::gui state drift from leaving
        // stale or vanished labels after incidental clicks.
        WickedUI::UpdateFrontDoorHost(m_sink);

        // Forward real pointer events to the shim host. wi::gui hit-tests
        // GetPointer() directly against widget rects in SetPos space (the
        // projector's logical UI pixels), so the pointer needs NO DPI conversion.
        if (pointerMoved)
            WickedUI::DispatchFrontDoorMouseMove(pointer.x, pointer.y);
        if (mousePressed)
            WickedUI::DispatchFrontDoorMouseDown(pointer.x, pointer.y);
        if (secondaryPressed)
            WickedUI::DispatchFrontDoorSecondaryClick(pointer.x, pointer.y);
        if (mouseReleased)
            WickedUI::DispatchFrontDoorMouseUp(pointer.x, pointer.y);

        m_lastPointerX = pointer.x;
        m_lastPointerY = pointer.y;
        m_hasLastPointer = true;

        // Decide what changed. A STRUCTURE change (mount/unmount) needs a rebuild
        // (deferred to next frame's guarded top). A VALUE-only change updates the
        // existing widgets in place — cheap, and safe even mid-press.
        const std::uint64_t structureToken = WickedUI::FrontDoorStructureToken();
        const std::uint64_t valueToken = WickedUI::FrontDoorStateToken();
        if (structureToken != m_lastStructureToken)
        {
            m_reprojectPending = true; // rebuilt (and tokens refreshed) when the guard clears
        }
        else if (valueToken != m_lastValueToken)
        {
            m_lastValueToken = valueToken;
        }
    }

private:
    // Clear + re-project the front-door host through the sink, then (re)attach the
    // widgets to the GUI. Add reversed: the projector emits backdrop→children
    // (back→front logically), but wi::gui treats the first-added widget as
    // frontmost, so reverse to keep the full-screen backdrop at the back.
    std::size_t Reproject()
    {
        m_sink.Clear();
        const std::size_t n = WickedUI::ProjectFrontDoorHost(m_sink);
        for (auto it = m_sink.owned.rbegin(); it != m_sink.owned.rend(); ++it)
            GetGUI().AddWidget(it->get());
        return n;
    }

    WiGuiSink m_sink;
    std::unique_ptr<DmoGodClientPanelWindowFactory> m_godClientPanelFactory;
    bool m_reprojectPending = false;
    bool m_waitForIdleMouseFrame = false;
    bool m_hasLastPointer = false;
    float m_lastPointerX = 0.0f;
    float m_lastPointerY = 0.0f;
    std::uint64_t m_lastStructureToken = 0; // last built screen-stack structure
    std::uint64_t m_lastValueToken = 0;     // last applied value snapshot
};

} // namespace

namespace DmoClient {

wi::RenderPath2D& FrontDoorPath()
{
    static DmoFrontDoorPath path;
    return path;
}

void SetBootScreen(std::uint32_t screenId)
{
    // Forward to the front-door host's boot-screen override. Keeps main.mm free of
    // DMO UI headers (it only knows DmoClient).
    WickedUI::SetFrontDoorBootScreen(screenId);
}

} // namespace DmoClient
