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
#include "Inventory/WickedInventoryRuntimeServices.h"
#include "Inventory/WickedItemFramingCatalog.h"
#include "wiGraphicsDevice_Metal.h"
#include "wiRenderPath3D.h"
#include "wiScene.h"
#include "wiPhysics.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

// Build the item-preview camera from a resolved framing (WICKED-UI-03D §8 Phase A framing
// catalog). Framing (fov/orbit/distance-margin/floor-drop) comes from the portable
// WickedItemFramingCatalog; here we only turn the engine-neutral solution into a
// CameraComponent. Default-constructed params = the catalog's three-quarter default.
static wi::scene::CameraComponent BuildBakeCamera(const XMFLOAT3& boundsCenter,
	const float boundsRadius,
	const int width,
	const int height,
	const WickedInventory::ItemFramingParams& framing = {})
{
	const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
	const WickedInventory::ItemFramingSolution sol =
		WickedInventory::WickedItemFramingCatalog::Solve(framing, boundsRadius, aspect);

	const XMFLOAT3 cameraPosition = XMFLOAT3(
		boundsCenter.x + sol.eyeOffsetX,
		boundsCenter.y + sol.eyeOffsetY,
		boundsCenter.z + sol.eyeOffsetZ);
	const XMFLOAT3 lookAt = XMFLOAT3(
		boundsCenter.x,
		boundsCenter.y + sol.atOffsetY,
		boundsCenter.z);

	wi::scene::TransformComponent cameraTransform;
	const XMVECTOR eye = XMLoadFloat3(&cameraPosition);
	const XMVECTOR at = XMLoadFloat3(&lookAt);
	const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
	const XMMATRIX viewInv = XMMatrixInverse(nullptr, view);
	XMStoreFloat4x4(&cameraTransform.world, viewInv);

	wi::scene::CameraComponent camera;
	camera.fov = sol.fovRadians;
	camera.width = static_cast<float>(width);
	camera.height = static_cast<float>(height);
	camera.zNearP = 0.05f;
	camera.zFarP = sol.distance + boundsRadius * 4.0f;
	camera.TransformCamera(cameraTransform);
	camera.UpdateCamera();
	return camera;
}

static std::string ResolveDmoClientMacShaderPath()
{
	namespace fs = std::filesystem;
	const fs::path executablePath = wi::helper::GetExecutablePath();
	const fs::path shaderPath = executablePath.parent_path() / "shaders" / "metal";
	return shaderPath.string() + "/";
}

static wi::graphics::Texture CaptureComposedBakeTexture(const wi::RenderPath3D& path, int width, int height)
{
	using namespace wi::graphics;
	GraphicsDevice* device = wi::graphics::GetDevice();

	Texture composed;
	TextureDesc desc;
	desc.bind_flags = BindFlag::RENDER_TARGET | BindFlag::SHADER_RESOURCE;
	desc.format = Format::R8G8B8A8_UNORM_SRGB;
	desc.width = static_cast<uint32_t>(std::max(1, width));
	desc.height = static_cast<uint32_t>(std::max(1, height));
	device->CreateTexture(&desc, nullptr, &composed);

	CommandList cmd = device->BeginCommandList();
	RenderPassImage rp[] = {
		RenderPassImage::RenderTarget(&composed, RenderPassImage::LoadOp::CLEAR)
	};
	device->RenderPassBegin(rp, 1, cmd);

	Viewport vp;
	vp.width = static_cast<float>(desc.width);
	vp.height = static_cast<float>(desc.height);
	device->BindViewports(1, &vp, cmd);

	wi::graphics::Rect rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = static_cast<int32_t>(desc.width);
	rect.bottom = static_cast<int32_t>(desc.height);
	device->BindScissorRects(1, &rect, cmd);

	path.Compose(cmd);

	device->RenderPassEnd(cmd);
	device->SubmitCommandLists();
	return composed;
}

// Sample an arbitrary HDR source texture (e.g. a scene camera's render_to_texture
// target, filled by RenderPath3D::RenderCameraComponents) into an 8-bit sRGB target
// via a full-screen image draw, then hand that back for CPU download. This is the
// readback that actually contains the baked object: RenderCameraComponents renders
// each render-to-texture scene camera into its OWN rendertarget_render, NOT into the
// main-view GetRenderResult3D() that path.Compose() blits. Same "resolve the HDR
// target to a displayable image" step the editor thumbnail does; also sidesteps the
// Metal-fork bug where saveTextureToFile can't download a packed-HDR-float texture.
static wi::graphics::Texture CaptureCameraTargetTexture(
	const wi::graphics::Texture& src, int width, int height)
{
	using namespace wi::graphics;
	GraphicsDevice* device = wi::graphics::GetDevice();
	if (!src.IsValid())
		return {};

	Texture resolved;
	TextureDesc desc;
	desc.bind_flags = BindFlag::RENDER_TARGET | BindFlag::SHADER_RESOURCE;
	desc.format = Format::R8G8B8A8_UNORM_SRGB;
	desc.width = static_cast<uint32_t>(std::max(1, width));
	desc.height = static_cast<uint32_t>(std::max(1, height));
	device->CreateTexture(&desc, nullptr, &resolved);

	CommandList cmd = device->BeginCommandList();
	RenderPassImage rp[] = {
		RenderPassImage::RenderTarget(&resolved, RenderPassImage::LoadOp::CLEAR)
	};
	device->RenderPassBegin(rp, 1, cmd);

	Viewport vp;
	vp.width = static_cast<float>(desc.width);
	vp.height = static_cast<float>(desc.height);
	device->BindViewports(1, &vp, cmd);

	wi::image::Params params;
	params.enableFullScreen();       // stretch src across the whole target
	params.blendFlag = wi::enums::BLENDMODE_OPAQUE;
	wi::image::Draw(&src, params, cmd);

	device->RenderPassEnd(cmd);
	device->SubmitCommandLists();
	return resolved;
}

static void SaveBakeDebugTargets(wi::RenderPath3D& path,
	const wi::graphics::Texture& composedTexture,
	const std::string& outPath)
{
	namespace fs = std::filesystem;
	if (std::getenv("DMO_ITEM_BAKE_DEBUG") == nullptr)
		return;

	const fs::path base(outPath);
	const fs::path stem = base.parent_path() / base.stem();
	const fs::path raw3D = stem.string() + ".raw3d.png";
	const fs::path overlay2D = stem.string() + ".overlay2d.png";
	const fs::path composed2D = stem.string() + ".composed2d.png";
	const fs::path alpha2D = stem.string() + ".alpha2d.png";

	(void)wi::helper::saveTextureToFile(path.GetRenderResult3D(), raw3D.string());
	(void)wi::helper::saveTextureToFile(path.GetRenderResult2D(), overlay2D.string());
	(void)wi::helper::saveTextureToFile(composedTexture, composed2D.string());
	(void)wi::helper::saveTextureToFile(path.CreateScreenshotWithAlphaBackground(), alpha2D.string());
}

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
	wi::renderer::SetShaderPath(ResolveDmoClientMacShaderPath());

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

	CommandList compose = device->BeginCommandList();
	path.Compose(compose);
	device->SubmitCommandLists();

	// 5. Read back the offscreen 2D result → PNG.
	const bool ok = wi::helper::saveTextureToFile(path.GetRenderResult2D(), outPath);
	std::fprintf(stderr, "[DmoClient] offscreen shot %s -> %s\n", ok ? "OK" : "FAIL", outPath);
	wi::jobsystem::ShutDown();
	return ok ? 0 : 1;
}

// Offscreen 3D ITEM-BAKE spike (DMO_ITEM_BAKE=<out.png>, DMO_ITEM_BAKE_MODEL=<path>).
//
// WICKED-UI-03D P0 de-risking: prove the RTT icon-bake path on THIS Metal build
// before wiring cache→sink→projector. Loads a model into a standalone scene, frames
// a camera on its bounds, renders it through a real wi::RenderPath3D into that path's
// own offscreen 3D target, and writes the result to a PNG. Same headless device +
// readback harness as RunOffscreenShot; the only new surface is the 3D scene + path.
//
// Ground truth = a PNG showing a rendered 3D object. If the 3D pipeline's Metal PSOs
// are absent (as several eager 3D subsystems are on this build), the device's
// null-shader guards no-op the draw and we get an empty frame rather than a crash —
// which is itself the signal that the object shaders need provisioning.
static int RunOffscreenItemBake(const char* outPath)
{
	using namespace wi::graphics;

	const char* modelEnv = std::getenv("DMO_ITEM_BAKE_MODEL");
	if (modelEnv == nullptr || modelEnv[0] == '\0')
	{
		std::fprintf(stderr, "[DmoClient] DMO_ITEM_BAKE requires DMO_ITEM_BAKE_MODEL=<path to .wiscene/.gltf>\n");
		return 2;
	}
	const std::string modelPath = modelEnv;

	// 0-2. Same headless device + subsystem bring-up as RunOffscreenShot.
	wi::renderer::SetShaderPath(ResolveDmoClientMacShaderPath());
	static std::unique_ptr<GraphicsDevice> device =
		std::make_unique<GraphicsDevice_Metal>(ValidationMode::Disabled, GPUPreference::Discrete);
	GetDevice() = device.get();
	// Full engine bring-up, exactly as wi::Application::Initialize() does. The windowed
	// DMO_3D_PROBE proved the object-draw path renders through the real loop — and the
	// only material difference from this headless harness was initialization: the minimal
	// 4-subsystem init (renderer/texturehelper/image/font) leaves object rendering with
	// missing dependencies, so DrawScene produced nothing. Immediate variant blocks until
	// every system is up (no async wait needed for a one-shot bake).
	wi::initializer::InitializeComponentsImmediate();

	// Icon-bake scenes are static — never simulate physics. This also avoids the
	// eager Jolt init that Scene::Update() would otherwise trigger (BodyManager::Init
	// faults because we bypass wi::physics::Initialize() in this headless harness).
	wi::physics::SetEnabled(false);

	// 3. Standalone scene + the item model. attached=true so we could transform the
	//    root later; here identity is fine.
	wi::scene::Scene scene;
	if (modelPath == "builtin:cube")
	{
		scene.Entity_CreateCube("builtin.cube");
	}
	else
	{
		wi::scene::LoadModel(scene, modelPath, XMMatrixIdentity(), true);
	}
	scene.Update(0.0f); // populate transforms + scene.bounds
	if (scene.objects.GetCount() == 0)
	{
		std::fprintf(stderr, "[DmoClient] item-bake: model loaded 0 objects (%s)\n", modelPath.c_str());
		return 1;
	}

	const XMFLOAT3 center = scene.bounds.getCenter();
	const float radius = std::max(0.001f, scene.bounds.getRadius());

	// Flat studio lighting (spec §3.1): a bright, even ambient via a WeatherComponent
	// makes the icon legible with no dependence on sky/IBL/point-light falloff. A
	// scene with no weather has zero ambient → the object renders black.
	{
		wi::scene::WeatherComponent& weather = scene.weathers.Create(wi::ecs::CreateEntity());
		weather.ambient = XMFLOAT3(1.0f, 1.0f, 1.0f);
	}
	// A key point light adds a little form on top of the flat ambient. Point-light
	// intensity is physical, so scale it up for the small studio distance.
	scene.Entity_CreateLight("bake.key",
		XMFLOAT3(center.x + radius * 2.0f, center.y + radius * 3.0f, center.z + radius * 2.0f),
		XMFLOAT3(1.0f, 1.0f, 1.0f), /*intensity*/ radius * radius * 5000.0f + 5000.0f,
		/*range*/ radius * 40.0f, wi::scene::LightComponent::POINT);
	scene.Update(0.0f);

	// 4-5. Drive a RenderPath3D with the bake camera as its MAIN-VIEW camera — the exact
	//    configuration proven to render by the windowed DMO_3D_PROBE. (An earlier attempt
	//    used a render_to_texture scene camera + RenderCameraComponents; that path never
	//    produced pixels headless. The main-view path does, now that full engine init runs.)
	const int bakeW = 256, bakeH = 256;
	wi::scene::CameraComponent mainViewCam = BuildBakeCamera(center, radius, bakeW, bakeH);
	wi::RenderPath3D path;
	path.scene = &scene;
	path.camera = &mainViewCam;
	const char* expEnv = std::getenv("DMO_BAKE_EXPOSURE");
	path.setExposure(expEnv != nullptr ? static_cast<float>(std::atof(expEnv)) : 1.0f);
	path.setBloomEnabled(false);
	path.setEyeAdaptionEnabled(false); // fixed exposure — no auto-adapt to a mostly-empty frame
	// Match wi::Application::Run() ordering exactly (the proven windowed path): Load() once,
	// then init() EVERY frame before PreUpdate. The one-shot init-before-Load ordering used
	// earlier left the 3D render targets unallocated at first render → nothing drawn.
	path.Load();
	for (int i = 0; i < 4; ++i)
	{
		path.init(static_cast<float>(bakeW), static_cast<float>(bakeH), 96.0f);
		path.PreUpdate();
		path.Update(1.0f / 60.0f);
		path.PostUpdate();
		path.PreRender();
		path.Render();
		path.PostRender();
		device->SubmitCommandLists();
	}

	// 6. Read back via a GPU RESOLVE to an 8-bit sRGB target, NOT the HDR scene target.
	//    saveTextureToFile cannot download the R11G11B10_FLOAT scene target on this Metal
	//    fork (yields all-zero), but GPU sampling of it is fine — so Compose the tonemapped
	//    main-view 3D result (GetRenderResult3D) into an R8G8B8A8_UNORM_SRGB target and read
	//    THAT. This matches the editor thumbnail's display-image resolve.
	Texture composed = CaptureComposedBakeTexture(path, bakeW, bakeH);
	if (std::getenv("DMO_ITEM_BAKE_DEBUG") != nullptr)
		SaveBakeDebugTargets(path, composed, outPath);
	const bool ok = composed.IsValid() && wi::helper::saveTextureToFile(composed, outPath);
	std::fprintf(stderr, "[DmoClient] item-bake %s -> %s (model=%s, %d objs, r=%.3f)\n",
		ok ? "OK" : "FAIL", outPath, modelPath.c_str(),
		static_cast<int>(scene.objects.GetCount()), radius);
	wi::jobsystem::ShutDown();
	return ok ? 0 : 1;
}

static std::string BuildInventoryPreviewImage(const WickedInventory::InventoryPreviewRequest& request);
// Render one item to a PNG THROUGH the engine — must be called inside a live
// wi::Application frame (the WE Editor's CameraPreview drives a preview RenderPath3D
// the same way). Defined after BuildInventoryPreviewImage; used by DmoApplication::Render().
static bool RenderItemPreviewToFile(const std::string& modelPath, const std::string& outPath,
                                    int width, int height);

extern bool running; // defined below; the run loop exits when this goes false

// Load + activate the DMO front-door path once the engine is initialized
// (the engine never auto-calls RenderPath::Load — see Editor::Initialize).
class DmoApplication : public wi::Application
{
public:
	void Initialize() override
	{
		wi::Application::Initialize();
		WickedInventory::SetInventoryPreviewImageResolver(
			[](const WickedInventory::InventoryPreviewRequest& request) {
				return BuildInventoryPreviewImage(request);
			});

		// DMO_3D_PROBE: windowed 3D-render sanity check. Instead of the 2D front door,
		// stand up a RenderPath3D with a lit item (cube, or DMO_ITEM_BAKE_MODEL=<path>) and
		// let the real wi::Application::Run() loop drive it. This is the ENGINE-FIRST way to
		// render a 3D item preview, exactly as the WE Editor's CameraPreview does: give a
		// RenderPath3D a scene + camera and let the real frame loop tick it — never hand-roll
		// the frame drive. (The prior headless hand-pump produced only sky; the real loop
		// renders the object because it completes the GPU-driven visibility→draw job graph
		// that a hand-pump on this Metal fork does not.)
		//   DMO_3D_PROBE            → just view it in the window (visual sanity).
		//   DMO_ITEM_CAPTURE=<png>  → view it, then capture the rendered result to <png> and
		//                             quit. This is the in-loop icon bake: the same render the
		//                             production IIconBakeSink will run inside the live client.
		const char* capturePng = std::getenv("DMO_ITEM_CAPTURE");
		if (std::getenv("DMO_3D_PROBE") != nullptr || capturePng != nullptr)
		{
			const char* model = std::getenv("DMO_ITEM_BAKE_MODEL");
			if (model != nullptr && model[0] != '\0' && std::string(model) != "builtin:cube")
				wi::scene::LoadModel(m_itemScene, model, XMMatrixIdentity(), true);
			else
				m_itemScene.Entity_CreateCube("item.cube");
			m_itemScene.Update(0.0f);

			const XMFLOAT3 center = m_itemScene.bounds.getCenter();
			const float radius = std::max(0.001f, m_itemScene.bounds.getRadius());
			{
				wi::scene::WeatherComponent& weather = m_itemScene.weathers.Create(wi::ecs::CreateEntity());
				weather.ambient = XMFLOAT3(1.0f, 1.0f, 1.0f);
			}
			m_itemScene.Entity_CreateLight("item.key",
				XMFLOAT3(center.x + radius * 2.0f, center.y + radius * 3.0f, center.z + radius * 2.0f),
				XMFLOAT3(1.0f, 1.0f, 1.0f), radius * radius * 5000.0f + 5000.0f,
				radius * 40.0f, wi::scene::LightComponent::POINT);
			m_itemScene.Update(0.0f);

			m_itemCam = BuildBakeCamera(center, radius, 1280, 800);
			m_itemPath.scene = &m_itemScene;
			m_itemPath.camera = &m_itemCam;
			m_itemPath.setExposure(1.0f);
			m_itemPath.Load();
			ActivatePath(&m_itemPath);
			if (capturePng != nullptr)
			{
				m_captureMode = true;
				m_capturePath = capturePng;
			}
			std::fprintf(stderr, "[DmoClient] item preview active (%d objects, r=%.3f)%s\n",
				static_cast<int>(m_itemScene.objects.GetCount()), radius,
				m_captureMode ? " [capture]" : "");
			return;
		}

		wi::RenderPath2D& path = DmoClient::FrontDoorPath();
		path.Load();
		ActivatePath(&path);
	}

	// Queue an inventory item to be rendered to a PNG. Called (on the main thread, during
	// Update) by the inventory preview-image resolver; drained in Render() so the actual
	// GPU render happens inside the live engine frame.
	void EnqueuePreview(std::string modelPath, std::string outPath, int width, int height)
	{
		m_previewJobs.push_back(PreviewJob{ std::move(modelPath), std::move(outPath), width, height });
	}

	void Render() override
	{
		wi::Application::Render();

		// Inventory icon bake: render at most one queued item preview per frame, THROUGH the
		// engine, inside the live frame (this is the WE Editor's CameraPreview approach — a
		// preview RenderPath3D driven within Application's frame). Bake-once: the result is a
		// PNG the inventory grid slot then displays via its "image" attribute.
		if (!m_previewJobs.empty())
		{
			const PreviewJob job = m_previewJobs.front();
			m_previewJobs.erase(m_previewJobs.begin());
			const bool ok = RenderItemPreviewToFile(job.modelPath, job.outPath, job.width, job.height);
			std::fprintf(stderr, "[DmoClient] inventory icon bake %s -> %s\n",
				ok ? "OK" : "FAIL", job.outPath.c_str());
		}

		// One-shot standalone capture (DMO_ITEM_CAPTURE): the active path IS the item path.
		if (m_captureMode && !m_captured)
		{
			if (++m_captureFrames < 3) // let init(canvas)/targets settle
				return;
			const wi::graphics::Texture* rt = m_itemPath.GetLastPostprocessRT();
			if (rt != nullptr && rt->IsValid())
			{
				wi::graphics::Texture out =
					CaptureCameraTargetTexture(*rt, static_cast<int>(rt->desc.width), static_cast<int>(rt->desc.height));
				const bool ok = out.IsValid() && wi::helper::saveTextureToFile(out, m_capturePath);
				std::fprintf(stderr, "[DmoClient] in-loop item capture %s -> %s (%ux%u)\n",
					ok ? "OK" : "FAIL", m_capturePath.c_str(), rt->desc.width, rt->desc.height);
			}
			else
			{
				std::fprintf(stderr, "[DmoClient] in-loop item capture FAIL: no valid render result\n");
			}
			m_captured = true;
			running = false; // one-shot: exit the run loop
		}
	}

private:
	struct PreviewJob
	{
		std::string modelPath;
		std::string outPath;
		int width = 128;
		int height = 128;
	};
	std::vector<PreviewJob> m_previewJobs;

	wi::scene::Scene m_itemScene;
	wi::scene::CameraComponent m_itemCam;
	wi::RenderPath3D m_itemPath;
	bool m_captureMode = false;
	bool m_captured = false;
	int m_captureFrames = 0;
	std::string m_capturePath;
};

DmoApplication application;
bool running = true;

static std::string ResolveInventoryDemoAssetPath(const std::string& stableKey)
{
	if (stableKey == "asset.7001")
		return "../Content/models/DamagedHelmet.glb";
	if (stableKey == "asset.7002")
		return "../Content/models/teapot.wiscene";
	if (stableKey == "asset.7003")
		return "../Content/models/cube.wiscene";
	if (stableKey == "asset.7004")
		return "../Content/models/CesiumMan.glb";
	return {};
}

static std::string BuildInventoryPreviewImage(const WickedInventory::InventoryPreviewRequest& request)
{
	namespace fs = std::filesystem;

	std::string modelPath = request.assetPath.empty() ? ResolveInventoryDemoAssetPath(request.stableKey)
	                                                  : request.assetPath;
	if (modelPath.empty())
		return {};

	const fs::path cacheRoot = fs::temp_directory_path() / "dmo-wicked-client" / "inventory-previews";
	std::error_code ec;
	fs::create_directories(cacheRoot, ec);
	std::string sanitized = request.stableKey;
	for (char& ch : sanitized)
	{
		const bool ok =
			(ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9') ||
			ch == '.' || ch == '_' || ch == '-';
		if (!ok)
			ch = '_';
	}
	const fs::path outPath = cacheRoot / (sanitized + ".png");
	if (fs::exists(outPath))
		return outPath.string();   // already baked — hand the slot its icon

	// Not baked yet: enqueue an in-frame render (drained by DmoApplication::Render, which is
	// the only place the engine's frame is live) and report "pending" (empty). The inventory
	// controller retries the resolver each frame until the PNG exists. Engine-first: we never
	// hand-pump a RenderPath3D outside the frame loop.
	const int bakeW = std::max(64, request.width);
	const int bakeH = std::max(64, request.height);
	// Dedup: the controller re-calls the resolver every frame while pending, so enqueue each
	// item's render exactly once.
	static std::unordered_set<std::string> s_enqueued;
	if (s_enqueued.insert(outPath.string()).second)
		application.EnqueuePreview(modelPath, outPath.string(), bakeW, bakeH);
	return {};
}

// Render a single item to a PNG through the engine. MUST run inside a live wi::Application
// frame (called only from DmoApplication::Render) — this mirrors the WE Editor's
// CameraPreview, which drives a preview RenderPath3D within the running frame and reads back
// GetLastPostprocessRT. Hand-pumping this outside a frame does not render on this Metal fork.
static bool RenderItemPreviewToFile(const std::string& modelPath, const std::string& outPath,
                                    int width, int height)
{
	using namespace wi::graphics;

	wi::scene::Scene scene;
	if (modelPath == "builtin:cube")
		scene.Entity_CreateCube("preview.cube");
	else
		wi::scene::LoadModel(scene, modelPath, XMMatrixIdentity(), true);
	scene.Update(0.0f);
	if (scene.objects.GetCount() == 0)
		return false;

	const XMFLOAT3 center = scene.bounds.getCenter();
	const float radius = std::max(0.001f, scene.bounds.getRadius());
	{
		wi::scene::WeatherComponent& weather = scene.weathers.Create(wi::ecs::CreateEntity());
		weather.ambient = XMFLOAT3(1.0f, 1.0f, 1.0f);
	}
	scene.Entity_CreateLight("inventory.bake.key",
		XMFLOAT3(center.x + radius * 2.0f, center.y + radius * 3.0f, center.z + radius * 2.0f),
		XMFLOAT3(1.0f, 1.0f, 1.0f), radius * radius * 5000.0f + 5000.0f,
		radius * 40.0f, wi::scene::LightComponent::POINT);
	scene.Update(0.0f);

	wi::scene::CameraComponent cam = BuildBakeCamera(center, radius, width, height);
	wi::RenderPath3D path;
	path.scene = &scene;
	path.camera = &cam;
	path.setExposure(1.0f);
	path.setBloomEnabled(false);
	path.setEyeAdaptionEnabled(false);
	path.Load();
	for (int i = 0; i < 3; ++i) // Load() once, init() every frame — matches Application::Run()
	{
		path.init(static_cast<float>(width), static_cast<float>(height), 96.0f);
		path.PreUpdate();
		path.Update(1.0f / 60.0f);
		path.PostUpdate();
		path.PreRender();
		path.Render();
		path.PostRender();
		GetDevice()->SubmitCommandLists();
	}

	const Texture* rt = path.GetLastPostprocessRT();
	if (rt == nullptr || !rt->IsValid())
		return false;
	const Texture out = CaptureCameraTargetTexture(*rt, width, height);
	return out.IsValid() && wi::helper::saveTextureToFile(out, outPath);
}

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

	// Headless 3D item-icon bake spike (WICKED-UI-03D P0 de-risk).
	if (const char* bakePath = std::getenv("DMO_ITEM_BAKE"))
	{
		@autoreleasepool {
			return RunOffscreenItemBake(bakePath);
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
