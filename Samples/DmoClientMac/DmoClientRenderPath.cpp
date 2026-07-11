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
        widget->SetColor(wi::Color::fromFloat4(XMFLOAT4(v.colorR, v.colorG, v.colorB, v.colorA * v.alpha)));

        wi::gui::Widget* raw = widget.get();
        if (gui != nullptr)
            gui->AddWidget(raw);
        owned.push_back(std::move(widget));
        return raw;
    }
};

class DmoFrontDoorPath final : public wi::RenderPath2D
{
public:
    void Load() override
    {
        wi::RenderPath2D::Load();
        m_sink.gui = &GetGUI();
        const std::size_t n = WickedUI::ProjectFrontDoorHost(m_sink);
        std::fprintf(stderr, "[DmoClient] auth screen projected: %zu widgets\n", n);
    }

private:
    WiGuiSink m_sink;
};

} // namespace

namespace DmoClient {

wi::RenderPath2D& FrontDoorPath()
{
    static DmoFrontDoorPath path;
    return path;
}

} // namespace DmoClient
