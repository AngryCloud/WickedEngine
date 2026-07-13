// external/WickedEngine/Samples/DmoClientMac/main.mm
//
// MACRENDER-01 — DMO WICKED client macOS window bring-up (spike).
//
// Proves the DMO client can link the in-tree WickedEngine and open a real
// Metal-backed window on Apple Silicon, running the wi::Application frame loop.
// Modeled on Samples/Template_MacOS/main.mm. This is a BRING-UP HARNESS: it does
// NOT yet touch the DMO UI substrate or the WickedUIWidget->wi::gui projector
// (that lands in later MACRENDER increments, which resolve the shim-vs-real
// header seam). For now: window + engine loop + info display only.
//
// NOTE: intentionally lives under the vendored engine's Samples/ so it links the
// WickedEngine CMake target directly (transitive Metal/SDL2/dxc/LUA deps for
// free), mirroring the Template_* samples. It is expected to be relocated into
// the DMO repo proper once the substrate/projector join the two build trees.

#include "WickedEngine.h"
#include "DmoClientRenderPath.h"
#include "wiGraphicsDevice_Metal.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <cstdlib>
#include <cstdio>

// Offscreen visual-verification path (DMO_UI_SHOT=<out.png>).
//
// Mac is a first-class DMO client target, so every UI screen must be verifiable
// on this machine — including from an automated/agent/CI context that has no
// WindowServer connection (a non-GUI shell cannot attach an NSWindow). We do NOT
// need a window to see pixels: the Metal device creates fine headlessly and
// RenderPath2D renders its GUI into an OFFSCREEN target (rtFinal) before Compose
// ever touches a swapchain. So we create the device, load the front-door path,
// pump a few frames into rtFinal, and write it to a PNG. Same render code the
// windowed client runs — just captured instead of presented.
static int RunOffscreenShot(const char* outPath)
{
	using namespace wi::graphics;

	// 0. Point the shader loader at the precompiled Metal library dir. wi::Application
	//    does this in its device-init path (wiApplication.cpp, PLATFORM_APPLE); since we
	//    bypass Application we must do it ourselves, else pipelines get null shaders
	//    (no runtime dxcompiler on macOS) and CreatePipelineState segfaults.
	wi::renderer::SetShaderPath(wi::renderer::GetShaderPath() + "metal/");

	// 1. Metal device — no window/swapchain needed (only CreateSwapChain wants one).
	static std::unique_ptr<GraphicsDevice> device =
		std::make_unique<GraphicsDevice_Metal>(ValidationMode::Disabled, GPUPreference::Discrete);
	GetDevice() = device.get();

	// 2. Bring up the subsystems the front-door GUI needs. wi::renderer::Initialize()
	//    is REQUIRED: it sets wi::renderer's internal device pointer, which
	//    wi::renderer::LoadShader() gates on — image/font shaders won't load without it
	//    (null VS → CreatePipelineState segfault). Its 3D object PSOs are built lazily, so
	//    this alone is safe. We deliberately DO NOT init the eager 3D systems
	//    (ocean/particles/BVH/gaussian/hair/trail/physics): several of their Metal pipeline
	//    shaders are absent in this build, and the wi::gui sink (images + fonts) never
	//    touches them.
	wi::renderer::Initialize();
	wi::texturehelper::Initialize();
	wi::image::Initialize();
	wi::font::Initialize();
	wi::input::Initialize();

	// 3. Optional boot-screen selector (DMO_UI_SHOT_SCREEN=<k*ScreenId>): capture a
	//    specific front-door/window screen instead of the default (auth). Must be set
	//    before Load() triggers the one-time host mount.
	if (const char* screenEnv = std::getenv("DMO_UI_SHOT_SCREEN"))
	{
		const long id = std::strtol(screenEnv, nullptr, 10);
		if (id > 0)
		{
			DmoClient::SetBootScreen(static_cast<std::uint32_t>(id));
			std::fprintf(stderr, "[DmoClient] offscreen boot screen = %ld\n", id);
		}
	}

	// 4. Size the path canvas (the GUI + rtFinal derive their extent from it) and project.
	wi::RenderPath2D& path = DmoClient::FrontDoorPath();
	path.init(1280, 800, 96.0f);
	path.Load();

	// 4. Pump frames — controllers advance, then render GUI into rtFinal. wi::font
	//    rasterizes glyphs LAZILY: a glyph is queued when text is drawn (inside Render),
	//    and UpdateAtlas() rasterizes the queued glyphs for the NEXT frame. So UpdateAtlas
	//    must run every frame (as wi::Application does) — calling it only once up front
	//    leaves the atlas empty and text renders as blank fills. Pumping a handful of
	//    frames also lets deferred reprojection + in-place value updates settle.
	for (int i = 0; i < 8; ++i)
	{
		wi::font::UpdateAtlas(path.GetDPIScaling());
		path.PreUpdate();
		path.Update(1.0f / 60.0f);
		path.PostUpdate();
		path.PreRender();
		path.Render();
		path.PostRender();
		device->SubmitCommandLists();
	}

	// 5. Read back the offscreen 2D result → PNG.
	const bool ok = wi::helper::saveTextureToFile(path.GetRenderResult2D(), outPath);
	std::fprintf(stderr, "[DmoClient] offscreen shot %s -> %s\n", ok ? "OK" : "FAIL", outPath);
	wi::jobsystem::ShutDown();
	return ok ? 0 : 1;
}

// Load + activate the DMO front-door path once the engine is initialized
// (the engine never auto-calls RenderPath::Load — see Editor::Initialize).
class DmoApplication : public wi::Application
{
public:
	void Initialize() override
	{
		wi::Application::Initialize();
		wi::RenderPath2D& path = DmoClient::FrontDoorPath();
		path.Load();
		ActivatePath(&path);
	}
};

DmoApplication application;
bool running = true;

@interface DmoWindowDelegate : NSObject <NSWindowDelegate>
@end

int main(int argc, char* argv[])
{
	// Headless visual capture: no NSApplication / window at all.
	if (const char* shotPath = std::getenv("DMO_UI_SHOT"))
	{
		@autoreleasepool {
			return RunOffscreenShot(shotPath);
		}
	}

	@autoreleasepool {
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

		CGRect frame = (CGRect){ {100.0, 100.0}, {1280.0, 800.0} };
		NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
													   styleMask:(NSWindowStyleMaskTitled |
																  NSWindowStyleMaskClosable |
																  NSWindowStyleMaskMiniaturizable |
																  NSWindowStyleMaskResizable)
														 backing:NSBackingStoreBuffered
														   defer:NO];
		[window setTitle:@"DMO WICKED Client"];
		[window center];
		[window makeKeyAndOrderFront:nil];

		DmoWindowDelegate* delegate = [[DmoWindowDelegate alloc] init];
		[window setDelegate:delegate];

		[NSApp activateIgnoringOtherApps:YES];

		application.SetWindow((__bridge wi::platform::window_type)window);
		// The DMO front-door path is loaded + activated in DmoApplication::Initialize
		// (called by the engine once the device is ready on the first Run frame).

		// Info overlay so the window visibly proves the render loop is live.
		application.infoDisplay.active = true;
		application.infoDisplay.watermark = true;
		application.infoDisplay.fpsinfo = true;
		application.infoDisplay.resolution = true;
		application.infoDisplay.logical_size = true;

		while (running)
		{
			@autoreleasepool {
				NSEvent* event;
				while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
												   untilDate:[NSDate distantPast]
													  inMode:NSDefaultRunLoopMode
													 dequeue:YES]))
				{
					switch (event.type) {
						case NSEventTypeKeyDown:
							switch (event.keyCode) {
								case kVK_Delete:
									wi::gui::TextInputField::DeleteFromInput();
									break;
								case kVK_Return:
									break;
								default: {
									NSString* characters = event.characters;
									if (characters && characters.length > 0)
									{
										unichar c = [characters characterAtIndex:0];
										wchar_t wchar = (wchar_t)c;
										wi::gui::TextInputField::AddInput(wchar);
									}
								} break;
							}
							break;
						case NSEventTypeKeyUp:
							break;
						default:
							[NSApp sendEvent:event];
							break;
					}
				}

				application.Run();
			}
		}

		wi::jobsystem::ShutDown();
	}

	return 0;
}

@implementation DmoWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
	running = false;
}
- (void)windowDidResize:(NSNotification*)notification {
	NSWindow* nsWindow = (NSWindow*)notification.object;
	application.SetWindow((__bridge wi::platform::window_type)nsWindow);
}
@end
