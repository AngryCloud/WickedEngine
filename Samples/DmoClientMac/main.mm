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

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

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
