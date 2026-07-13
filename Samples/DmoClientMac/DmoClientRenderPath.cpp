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

#include <cstdio>
#include <memory>
#include <vector>

namespace {

// Frozen WickedUIWidgetType ordinals we special-case (see WickedUIWidgetTypes.h).
enum : int { kTypeText = 1, kTypeButton = 2 };

// Materializes each projected visual as a wi::gui widget and owns their lifetime
// (the GUI only borrows the raw pointers).
class WiGuiSink final : public WickedUI::IWickedUIWidgetVisualSink
{
public:
    wi::gui::GUI* gui = nullptr;
    std::vector<std::unique_ptr<wi::gui::Widget>> owned;

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

    void* CreateVisual(const WickedUI::WickedUIWidgetVisual& v, void* /*parent*/) override
    {
        // The projector already resolves absolute coordinates, so every widget is
        // added flat to the GUI at its absolute position (no wi parenting needed).
        std::unique_ptr<wi::gui::Widget> widget;
        const char* name = v.name ? v.name : "";
        if (v.typeOrdinal == kTypeButton)
        {
            auto b = std::make_unique<wi::gui::Button>();
            b->Create(name);
            widget = std::move(b);
        }
        else // Text label, or a panel/backdrop rendered as a colored label rect.
        {
            auto l = std::make_unique<wi::gui::Label>();
            l->Create(name);
            widget = std::move(l);
        }
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

class DmoFrontDoorPath final : public wi::RenderPath2D
{
public:
    void Load() override
    {
        wi::RenderPath2D::Load();
        m_sink.gui = &GetGUI();
        const std::size_t n = Reproject();
        m_lastStructureToken = WickedUI::FrontDoorStructureToken();
        m_lastValueToken = WickedUI::FrontDoorStateToken();
        std::fprintf(stderr, "[DmoClient] front door projected: %zu widgets\n", n);
    }

    void Update(float dt) override
    {
        // STRUCTURAL rebuild (screens mounted/unmounted) is the only path that tears
        // down + recreates wi::gui widgets. Do it at the top of the frame, and NEVER
        // while the mouse is held — freeing a widget wi::gui is tracking for a press
        // corrupts the GUI (this was the "UI disappears on click" failure). Value
        // changes (vitals/chat/status) go through the in-place path below instead, so
        // they never destroy a widget the user is pressing.
        const bool mouseHeld = wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        if (m_reprojectPending && !mouseHeld)
        {
            const std::size_t n = Reproject();
            std::fprintf(stderr, "[DmoClient] reprojected: %zu widgets\n", n);
            m_reprojectPending = false;
            m_lastStructureToken = WickedUI::FrontDoorStructureToken();
            m_lastValueToken = WickedUI::FrontDoorStateToken(); // fresh tree = current values
        }

        wi::RenderPath2D::Update(dt);

        // Advance controllers (Refresh/animation — e.g. the loading bar).
        WickedUI::TickFrontDoor(dt);

        // Forward a left-click press edge to the shim host. wi::gui hit-tests
        // GetPointer() directly against widget rects in SetPos space (the
        // projector's logical UI pixels), so the pointer needs NO DPI conversion.
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            const XMFLOAT4 p = wi::input::GetPointer();
            WickedUI::DispatchFrontDoorClick(p.x, p.y);
        }

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
            WickedUI::UpdateFrontDoorHost(m_sink); // in-place; no teardown
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
    bool m_reprojectPending = false;
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

} // namespace DmoClient
