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

    void* CreateVisual(const WickedUI::WickedUIWidgetVisual& v, void* /*parent*/) override
    {
        // The projector already resolves absolute coordinates, so every widget is
        // added flat to the GUI at its absolute position (no wi parenting needed).
        std::unique_ptr<wi::gui::Widget> widget;
        const char* name = v.name ? v.name : "";
        const char* text = v.text ? v.text : "";

        if (v.typeOrdinal == kTypeButton)
        {
            auto b = std::make_unique<wi::gui::Button>();
            b->Create(name);
            b->SetText(text);
            widget = std::move(b);
        }
        else // Text label, or a panel/backdrop rendered as a colored label rect.
        {
            auto l = std::make_unique<wi::gui::Label>();
            l->Create(name);
            l->SetText(v.typeOrdinal == kTypeText ? text : "");
            widget = std::move(l);
        }

        widget->SetPos(XMFLOAT2(v.x, v.y));
        widget->SetSize(XMFLOAT2(v.width, v.height));

        // The projector emits ONE resolved color per widget, but wi::gui widgets
        // have a distinct background fill (SetColor) and text color (font.params).
        // Interpret the color by kind so text isn't drawn as a filled bar:
        //   - Text  -> the color is the FONT color; background stays transparent
        //              so the panel behind shows through (dark panel + light text).
        //   - Button/panel/field -> the color fills the background; give buttons a
        //              light label so it reads against the dark surface.
        const wi::Color themed =
            wi::Color::fromFloat4(XMFLOAT4(v.colorR, v.colorG, v.colorB, v.colorA * v.alpha));
        if (v.typeOrdinal == kTypeText)
        {
            widget->SetColor(wi::Color::fromFloat4(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)));
            widget->font.params.color = themed;
        }
        else
        {
            widget->SetColor(themed);
            if (v.typeOrdinal == kTypeButton)
                widget->font.params.color = wi::Color::fromFloat4(XMFLOAT4(0.92f, 0.92f, 0.95f, 1.0f));
        }

        // NOTE: do NOT AddWidget here. wi::gui z-orders first-added as frontmost, but the
        // projector emits parent(backdrop)-before-children, so we defer and add reversed
        // (see DmoFrontDoorPath::Reproject) — else the backdrop paints over every child.
        wi::gui::Widget* raw = widget.get();
        owned.push_back(std::move(widget));
        return raw;
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
        m_lastToken = WickedUI::FrontDoorStateToken();
        std::fprintf(stderr, "[DmoClient] front door projected: %zu widgets\n", n);
    }

    void Update(float dt) override
    {
        // A state change last frame (screen transition, status text, or animation)
        // needs a rebuild: do it at the top of the frame, BEFORE the GUI updates,
        // so the teardown never races a press wi::gui is still tracking (freeing a
        // pressed widget mid-frame corrupts the GUI).
        if (m_reprojectPending)
        {
            const std::size_t n = Reproject();
            std::fprintf(stderr, "[DmoClient] reprojected: %zu widgets\n", n);
            m_reprojectPending = false;
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

        // One unified re-projection trigger: the state token changes on any screen
        // transition, status update, or animation frame.
        const std::uint64_t token = WickedUI::FrontDoorStateToken();
        if (token != m_lastToken)
        {
            m_lastToken = token;
            m_reprojectPending = true;
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
    std::uint64_t m_lastToken = 0; // last projected front-door state token
};

} // namespace

namespace DmoClient {

wi::RenderPath2D& FrontDoorPath()
{
    static DmoFrontDoorPath path;
    return path;
}

} // namespace DmoClient
