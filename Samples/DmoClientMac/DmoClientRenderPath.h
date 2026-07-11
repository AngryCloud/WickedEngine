#pragma once
// external/WickedEngine/Samples/DmoClientMac/DmoClientRenderPath.h
// WICKED-UI-03A-07B — the DMO front-door RenderPath (engine side).
//
// Returns a wi::RenderPath2D whose GUI is populated by projecting the DMO widget
// IR (via WickedUI::ProjectDemoScreen) through a wi::gui-backed visual sink.
// main.mm hands this to wi::Application::ActivatePath so the DMO screen renders.

namespace wi { class RenderPath2D; }

namespace DmoClient {

// Process-lifetime front-door RenderPath (built on first call). Pass to
// wi::Application::ActivatePath.
wi::RenderPath2D& FrontDoorPath();

} // namespace DmoClient
