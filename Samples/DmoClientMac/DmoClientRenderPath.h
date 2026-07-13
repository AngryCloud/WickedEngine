#pragma once
// external/WickedEngine/Samples/DmoClientMac/DmoClientRenderPath.h
// WICKED-UI-03A-07B — the DMO front-door RenderPath (engine side).
//
// Returns a wi::RenderPath2D whose GUI is populated by projecting the DMO widget
// IR (via WickedUI::ProjectDemoScreen) through a wi::gui-backed visual sink.
// main.mm hands this to wi::Application::ActivatePath so the DMO screen renders.

namespace wi { class RenderPath2D; }

#include <cstdint>

namespace DmoClient {

// Process-lifetime front-door RenderPath (built on first call). Pass to
// wi::Application::ActivatePath.
wi::RenderPath2D& FrontDoorPath();

// Override which front-door screen is mounted first (used by the offscreen capture
// path to boot directly to a chosen screen). Pass a WickedUI k*ScreenId; 0 = default
// (auth). Must be called before FrontDoorPath().Load(). Forwards to
// WickedUI::SetFrontDoorBootScreen so main.mm needs no DMO UI headers.
void SetBootScreen(std::uint32_t screenId);

} // namespace DmoClient
