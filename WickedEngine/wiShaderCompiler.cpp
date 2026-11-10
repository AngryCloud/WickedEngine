#include "wiShaderCompiler.h"
#include "wiBacklog.h"
#include "wiPlatform.h"
#include "wiHelper.h"
#include "wiArchive.h"
#include "wiUnorderedSet.h"
#include "wiUnorderedMap.h"

#include <mutex>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef PLATFORM_WINDOWS_DESKTOP
#define SHADERCOMPILER_ENABLED
#define SHADERCOMPILER_ENABLED_DXCOMPILER
#define SHADERCOMPILER_ENABLED_D3DCOMPILER
#include <wrl/client.h>
using namespace Microsoft::WRL;
#endif // _WIN32

#if defined(PLATFORM_LINUX) || defined(PLATFORM_APPLE)
#define SHADERCOMPILER_ENABLED
#define SHADERCOMPILER_ENABLED_DXCOMPILER
#define __RPC_FAR
#define ComPtr CComPtr
#include "Utility/dxc/WinAdapter.h"
#endif // defined(PLATFORM_LINUX) || defined(PLATFORM_APPLE)

#ifdef SHADERCOMPILER_ENABLED_DXCOMPILER
#include "Utility/dxc/dxcapi.h"
#include "wiGraphicsDevice_Vulkan.h"
#endif // SHADERCOMPILER_ENABLED_DXCOMPILER

#ifdef SHADERCOMPILER_ENABLED_D3DCOMPILER
#include <d3dcompiler.h>
#endif // SHADERCOMPILER_ENABLED_D3DCOMPILER

#if __has_include("wiShaderCompiler_XBOX.h")
#include "wiShaderCompiler_XBOX.h"
#define SHADERCOMPILER_XBOX_INCLUDED
#endif // __has_include("wiShaderCompiler_XBOX.h")

#if __has_include("wiShaderCompiler_PS5.h") && !defined(PLATFORM_PS5)
#include "wiShaderCompiler_PS5.h"
#define SHADERCOMPILER_PS5_INCLUDED
#endif // __has_include("wiShaderCompiler_PS5.h") && !PLATFORM_PS5

#ifdef PLATFORM_APPLE
#define SHADERCOMPILER_APPLE_INCLUDED
#include <metal_irconverter/metal_irconverter.h>
#include "wiGraphicsDevice_Metal.h"
#endif // PLATFORM_APPLE

using namespace wi::graphics;

namespace wi::shadercompiler
{

#ifdef SHADERCOMPILER_ENABLED_DXCOMPILER
	struct InternalState_DXC
	{
		DxcCreateInstanceProc DxcCreateInstance = nullptr;
		std::string version = "unavailable";

		InternalState_DXC(const std::string& modifier = "")
		{
#ifdef _WIN32
#ifdef SHADERCOMPILER_XBOX_INCLUDED
			wi::shadercompiler::xbox::GetSetSDKPath(); // Sets Xbox SDK path globally for DLL loading
#endif // SHADERCOMPILER_XBOX_INCLUDED
			const std::string library = "dxcompiler" + modifier + ".dll";
			HMODULE dxcompiler = wiLoadLibrary(library.c_str());
#elif defined(PLATFORM_LINUX)
			const std::string library = "./libdxcompiler" + modifier + ".so";
			HMODULE dxcompiler = wiLoadLibrary(library.c_str());
#elif defined(PLATFORM_APPLE)
			const std::string library = "./libdxcompiler" + modifier + ".dylib";
			HMODULE dxcompiler = wiLoadLibrary(library.c_str());
#endif
			if (dxcompiler != nullptr)
			{
				DxcCreateInstance = (DxcCreateInstanceProc)wiGetProcAddress(dxcompiler, "DxcCreateInstance");
				if (DxcCreateInstance != nullptr)
				{
					ComPtr<IDxcCompiler3> dxcCompiler;
					HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
					assert(SUCCEEDED(hr));
					ComPtr<IDxcVersionInfo> info;
					hr = dxcCompiler->QueryInterface(IID_PPV_ARGS(&info));
					assert(SUCCEEDED(hr));
					uint32_t minor = 0;
					uint32_t major = 0;
					hr = info->GetVersion(&major, &minor);
					assert(SUCCEEDED(hr));
					version = "dxc " + std::to_string(major) + "." + std::to_string(minor);
					ComPtr<IDxcVersionInfo2> info2;
					if (SUCCEEDED(dxcCompiler->QueryInterface(IID_PPV_ARGS(&info2))))
					{
						uint32_t commitCount = 0;
						char* commitHash = nullptr;
						if (SUCCEEDED(info2->GetCommitInfo(&commitCount, &commitHash)) && commitHash != nullptr)
						{
							version += " commit " + std::to_string(commitCount) + " " + commitHash;
						}
					}
					wi::backlog::post("wi::shadercompiler: loaded " + library + " (version: " + std::to_string(major) + "." + std::to_string(minor) + ")");
				}
			}
			else
			{
				wi::backlog::post("wi::shadercompiler: could not load library " + library, wi::backlog::LogLevel::Error);
#ifdef PLATFORM_LINUX
				wi::backlog::post(dlerror(), wi::backlog::LogLevel::Error); // print dlopen() error detail: https://linux.die.net/man/3/dlerror
#endif // PLATFORM_LINUX
			}

		}
	};
	inline InternalState_DXC& dxc_compiler()
	{
		static InternalState_DXC internal_state;
		return internal_state;
	}
	inline InternalState_DXC& dxc_compiler_xs()
	{
		static InternalState_DXC internal_state("_xs");
		return internal_state;
	}

	void Compile_DXCompiler(const CompilerInput& input, CompilerOutput& output)
	{
		InternalState_DXC& compiler_internal = input.format == ShaderFormat::HLSL6_XS ? dxc_compiler_xs() : dxc_compiler();
		if (compiler_internal.DxcCreateInstance == nullptr)
		{
			return;
		}

		ComPtr<IDxcUtils> dxcUtils;
		ComPtr<IDxcCompiler3> dxcCompiler;

		HRESULT hr = compiler_internal.DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		assert(SUCCEEDED(hr));
		hr = compiler_internal.DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		assert(SUCCEEDED(hr));

		if (dxcCompiler == nullptr)
		{
			return;
		}

		wi::vector<uint8_t> shadersourcedata;
		if (!wi::helper::FileRead(input.shadersourcefilename, shadersourcedata))
		{
			return;
		}

		// https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#dxcompiler-dll-interface

		wi::vector<std::wstring> args = {
			//L"-res-may-alias",
			//L"-flegacy-macro-expansion",
			//L"-no-legacy-cbuf-layout",
			//L"-pack-optimized", // this has problem with tessellation shaders: https://github.com/microsoft/DirectXShaderCompiler/issues/3362
			//L"-all-resources-bound",
			//L"-Gis", // Force IEEE strictness
			//L"-Gec", // Enable backward compatibility mode
			//L"-Ges", // Enable strict mode
			//L"-O0", // Optimization Level 0
			//L"-enable-16bit-types",
			L"-Wno-conversion",
		};

		if (has_flag(input.flags, Flags::DISABLE_OPTIMIZATION))
		{
			args.push_back(L"-Od");
		}
		
		if (has_flag(input.flags, Flags::KEEP_DEBUG_INFORMATION))
		{
			args.push_back(L"-Zi");
			args.push_back(L"-Qembed_debug");
		}

		switch (input.format)
		{
		case ShaderFormat::HLSL6:
		case ShaderFormat::HLSL6_XS:
			args.push_back(L"-rootsig-define"); args.push_back(L"WICKED_ENGINE_DEFAULT_ROOTSIGNATURE");
			if (has_flag(input.flags, Flags::STRIP_REFLECTION))
			{
				args.push_back(L"-Qstrip_reflect"); // only valid in HLSL6 compiler
			}
			break;
		case ShaderFormat::SPIRV:
			args.push_back(L"-spirv");
			args.push_back(L"-fspv-target-env=vulkan1.3");
			args.push_back(L"-fvk-use-dx-layout");
			args.push_back(L"-fvk-use-dx-position-w");
			args.push_back(L"-fvk-b-shift"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::VULKAN_BINDING_SHIFT_B)); args.push_back(L"0");
			args.push_back(L"-fvk-t-shift"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::VULKAN_BINDING_SHIFT_T)); args.push_back(L"0");
			args.push_back(L"-fvk-u-shift"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::VULKAN_BINDING_SHIFT_U)); args.push_back(L"0");
			args.push_back(L"-fvk-s-shift"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::VULKAN_BINDING_SHIFT_S)); args.push_back(L"0");

			// These require mutable descriptor extension which have issues on linux so it's not used:
			//args.push_back(L"-fvk-bind-sampler-heap"); args.push_back(L"0"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_SAMPLER));
			//args.push_back(L"-fvk-bind-resource-heap"); args.push_back(L"0"); args.push_back(std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_RESOURCE));

			// Non-mutable bindless descriptor heap bind points:
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_SAMPLER=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_SAMPLER));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_STORAGE_BUFFER=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_STORAGE_BUFFER));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_UNIFORM_TEXEL_BUFFER=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_UNIFORM_TEXEL_BUFFER));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_SAMPLED_IMAGE=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_SAMPLED_IMAGE));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_STORAGE_IMAGE=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_STORAGE_IMAGE));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_STORAGE_TEXEL_BUFFER=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_STORAGE_TEXEL_BUFFER));
			args.push_back(L"-D"); args.push_back(L"DESCRIPTOR_SET_BINDLESS_ACCELERATION_STRUCTURE=" + std::to_wstring((int)wi::graphics::GraphicsDevice_Vulkan::DESCRIPTOR_SET_BINDLESS_ACCELERATION_STRUCTURE));

			break;
		case ShaderFormat::METAL:
			args.push_back(L"-D"); args.push_back(L"__metal__");
			break;
		default:
			assert(0);
			return;
		}

		ShaderModel minshadermodel = input.minshadermodel;

		// global minshadermodel override:
		//minshadermodel = std::max(minshadermodel, ShaderModel::SM_6_2);

		if (input.format == ShaderFormat::HLSL6_XS)
		{
			minshadermodel = ShaderModel::SM_6_2;
			args.push_back(L"-enable-16bit-types");
		}
		
		if (input.format == ShaderFormat::METAL)
		{
			minshadermodel = std::max(minshadermodel, ShaderModel::SM_6_6);
		}

		args.push_back(L"-T");
		switch (input.stage)
		{
		case ShaderStage::MS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"ms_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"ms_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"ms_6_7");
				break;
			}
			break;
		case ShaderStage::AS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"as_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"as_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"as_6_7");
				break;
			}
			break;
		case ShaderStage::VS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"vs_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"vs_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"vs_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"vs_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"vs_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"vs_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"vs_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"vs_6_7");
				break;
			}
			break;
		case ShaderStage::HS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"hs_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"hs_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"hs_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"hs_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"hs_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"hs_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"hs_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"hs_6_7");
				break;
			}
			break;
		case ShaderStage::DS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"ds_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"ds_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"ds_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"ds_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"ds_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"ds_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"ds_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"ds_6_7");
				break;
			}
			break;
		case ShaderStage::GS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"gs_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"gs_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"gs_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"gs_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"gs_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"gs_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"gs_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"gs_6_7");
				break;
			}
			break;
		case ShaderStage::PS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"ps_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"ps_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"ps_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"ps_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"ps_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"ps_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"ps_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"ps_6_7");
				break;
			}
			break;
		case ShaderStage::CS:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"cs_6_0");
				break;
			case ShaderModel::SM_6_1:
				args.push_back(L"cs_6_1");
				break;
			case ShaderModel::SM_6_2:
				args.push_back(L"cs_6_2");
				break;
			case ShaderModel::SM_6_3:
				args.push_back(L"cs_6_3");
				break;
			case ShaderModel::SM_6_4:
				args.push_back(L"cs_6_4");
				break;
			case ShaderModel::SM_6_5:
				args.push_back(L"cs_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"cs_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"cs_6_7");
				break;
			}
			break;
		case ShaderStage::LIB:
			switch (minshadermodel)
			{
			default:
				args.push_back(L"lib_6_5");
				break;
			case ShaderModel::SM_6_6:
				args.push_back(L"lib_6_6");
				break;
			case ShaderModel::SM_6_7:
				args.push_back(L"lib_6_7");
				break;
			}
			break;
		default:
			assert(0);
			return;
		}

		for (auto& x : input.defines)
		{
			args.push_back(L"-D");
			wi::helper::StringConvert(x, args.emplace_back());
		}

		for (auto& x : input.include_directories)
		{
			args.push_back(L"-I");
			wi::helper::StringConvert(x, args.emplace_back());
		}

#ifdef SHADERCOMPILER_XBOX_INCLUDED
		if (input.format == ShaderFormat::HLSL6_XS)
		{
			wi::shadercompiler::xbox::AddArguments(input, args);
		}
#endif // SHADERCOMPILER_XBOX_INCLUDED

		// Entry point parameter:
		std::wstring wentry;
		wi::helper::StringConvert(input.entrypoint, wentry);
		args.push_back(L"-E");
		args.push_back(wentry.c_str());

		// Add source file name as last parameter. This will be displayed in error messages
		std::wstring wsource;
		wi::helper::StringConvert(wi::helper::GetFileNameFromPath(input.shadersourcefilename), wsource);
		args.push_back(wsource.c_str());

		DxcBuffer Source;
		Source.Ptr = shadersourcedata.data();
		Source.Size = shadersourcedata.size();
		Source.Encoding = DXC_CP_ACP;

		struct IncludeHandler final : public IDxcIncludeHandler
		{
			const CompilerInput* input = nullptr;
			CompilerOutput* output = nullptr;
			ComPtr<IDxcIncludeHandler> dxcIncludeHandler;

			HRESULT STDMETHODCALLTYPE LoadSource(
				_In_z_ LPCWSTR pFilename,                                 // Candidate filename.
				_COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource  // Resultant source object for included file, nullptr if not found.
			) override
			{
				HRESULT hr = dxcIncludeHandler->LoadSource(pFilename, ppIncludeSource);
				if (SUCCEEDED(hr))
				{
					std::string& filename = output->dependencies.emplace_back();
					wi::helper::StringConvert(pFilename, filename);
				}
				return hr;
			}
			HRESULT STDMETHODCALLTYPE QueryInterface(
				/* [in] */ REFIID riid,
				/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
			{
				return dxcIncludeHandler->QueryInterface(riid, ppvObject);
			}

			ULONG STDMETHODCALLTYPE AddRef(void) override
			{
				return 0;
			}
			ULONG STDMETHODCALLTYPE Release(void) override
			{
				return 0;
			}
		} includehandler;
		includehandler.input = &input;
		includehandler.output = &output;

		hr = dxcUtils->CreateDefaultIncludeHandler(&includehandler.dxcIncludeHandler);
		assert(SUCCEEDED(hr));

		wi::vector<const wchar_t*> args_raw;
		args_raw.reserve(args.size());
		for (auto& x : args)
		{
			args_raw.push_back(x.c_str());
		}

#ifndef _WIN32
		// work around https://github.com/microsoft/DirectXShaderCompiler/issues/7869
		static std::mutex locale_mut;
		static char* prev_locale;
		// we need to use a mutex anyway, so no point in using atomic_int
		static int scope = 0;

		{
			std::scoped_lock lock(locale_mut);
			if (scope++ == 0) {
				prev_locale = strdup(setlocale(LC_ALL, nullptr));
				setlocale(LC_ALL, "en_US.UTF-8");
			}
		}
#endif
		ComPtr<IDxcResult> pResults;
		hr = dxcCompiler->Compile(
			&Source,						// Source buffer.
			args_raw.data(),			// Array of pointers to arguments.
			(uint32_t)args.size(),		// Number of arguments.
			&includehandler,		// User-provided interface to handle #include directives (optional).
			IID_PPV_ARGS(&pResults)	// Compiler output status, buffer, and errors.
		);
#ifndef _WIN32
		{
			std::scoped_lock lock(locale_mut);
			if (--scope == 0) {
				setlocale(LC_ALL, prev_locale);
				free(prev_locale);
			}
		}
#endif
		assert(SUCCEEDED(hr));

		ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		hr = pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
		assert(SUCCEEDED(hr));
		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		{
			output.error_message = pErrors->GetStringPointer();
		}

		HRESULT hrStatus;
		hr = pResults->GetStatus(&hrStatus);
		assert(SUCCEEDED(hr));
		if (FAILED(hrStatus))
		{
			return;
		}

		ComPtr<IDxcBlob> pShader = nullptr;
		hr = pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);
		assert(SUCCEEDED(hr));
		if (pShader != nullptr)
		{
			output.dependencies.push_back(input.shadersourcefilename);
			output.shaderdata = (const uint8_t*)pShader->GetBufferPointer();
			output.shadersize = pShader->GetBufferSize();
			
			// keep the blob alive == keep shader pointer valid!
			auto internal_state = wi::allocator::make_shared<ComPtr<IDxcBlob>>();
			*internal_state = pShader;
			output.internal_state = internal_state;
			
#ifdef SHADERCOMPILER_APPLE_INCLUDED
			if (input.format == ShaderFormat::METAL)
			{
				static HMODULE irconverter = wiLoadLibrary("libmetalirconverter.dylib");
				assert(irconverter); // You must install the metal shader converter
				if (irconverter != nullptr)
				{
#define LINK_IR(name) using PFN_##name = decltype(&name); static PFN_##name name = (PFN_##name)wiGetProcAddress(irconverter, #name);
					LINK_IR(IRCompilerCreate)
					LINK_IR(IRCompilerSetMinimumGPUFamily)
					LINK_IR(IRCompilerSetCompatibilityFlags)
					LINK_IR(IRCompilerSetValidationFlags)
					LINK_IR(IRCompilerSetEntryPointName)
					LINK_IR(IRCompilerSetStageInGenerationMode)
					LINK_IR(IRObjectCreateFromDXIL)
					LINK_IR(IRCompilerAllocCompileAndLink)
					LINK_IR(IRErrorDestroy)
					LINK_IR(IRMetalLibBinaryCreate)
					LINK_IR(IRObjectGetMetalLibBinary)
					LINK_IR(IRMetalLibGetBytecodeSize)
					LINK_IR(IRMetalLibGetBytecode)
					LINK_IR(IRMetalLibBinaryDestroy)
					LINK_IR(IRObjectDestroy)
					LINK_IR(IRCompilerDestroy)
					LINK_IR(IRRootSignatureCreateFromDescriptor)
					LINK_IR(IRCompilerSetGlobalRootSignature)
					LINK_IR(IRCompilerEnableGeometryAndTessellationEmulation)
					//LINK_IR(IRRootSignatureDestroy)
					
					static IRDescriptorRange1 binding_resources[] =
					{
						{ .RangeType = IRDescriptorRangeTypeCBV, .BaseShaderRegister = arraysize(GraphicsDevice_Metal::RootLayout::root_cbvs), .RegisterSpace = 0, .OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend, .NumDescriptors = (arraysize(DescriptorBindingTable::CBV) - arraysize(GraphicsDevice_Metal::RootLayout::root_cbvs)), .Flags = IRDescriptorRangeFlagDataStaticWhileSetAtExecute },
						{ .RangeType = IRDescriptorRangeTypeSRV, .BaseShaderRegister = 0, .RegisterSpace = 0, .OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend, .NumDescriptors = arraysize(DescriptorBindingTable::SRV), .Flags = IRDescriptorRangeFlagDataStaticWhileSetAtExecute },
						{ .RangeType = IRDescriptorRangeTypeUAV, .BaseShaderRegister = 0, .RegisterSpace = 0, .OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend, .NumDescriptors = arraysize(DescriptorBindingTable::UAV), .Flags = IRDescriptorRangeFlagDataStaticWhileSetAtExecute },
					};
					static IRDescriptorRange1 binding_samplers[] =
					{
						{ .RangeType = IRDescriptorRangeTypeSampler, .BaseShaderRegister = 0, .RegisterSpace = 0, .OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend, .NumDescriptors = arraysize(DescriptorBindingTable::SAM), .Flags = IRDescriptorRangeFlagDescriptorsVolatile },
						{ .RangeType = IRDescriptorRangeTypeSampler, .BaseShaderRegister = wi::graphics::STATIC_SAMPLER_SLOT_BEGIN, .RegisterSpace = 0, .OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend, .NumDescriptors = arraysize(GraphicsDevice_Metal::StaticSamplerDescriptors::samplers), .Flags = IRDescriptorRangeFlagDescriptorsVolatile }, // static samplers workaround (**)
					};
					static IRRootParameter1 root_parameters[] =
					{
						{
							.ParameterType = IRRootParameterType32BitConstants,
							.Constants = { .ShaderRegister = 999, .Num32BitValues = arraysize(GraphicsDevice_Metal::RootLayout::constants), .RegisterSpace = 0 },
							.ShaderVisibility = IRShaderVisibilityAll
						},
						{
							.ParameterType = IRRootParameterTypeCBV,
							.Descriptor = { .ShaderRegister = 0, .RegisterSpace = 0, .Flags = IRRootDescriptorFlagNone },
							.ShaderVisibility = IRShaderVisibilityAll
						},
						{
							.ParameterType = IRRootParameterTypeCBV,
							.Descriptor = { .ShaderRegister = 1, .RegisterSpace = 0, .Flags = IRRootDescriptorFlagNone },
							.ShaderVisibility = IRShaderVisibilityAll
						},
						{
							.ParameterType = IRRootParameterTypeCBV,
							.Descriptor = { .ShaderRegister = 2, .RegisterSpace = 0, .Flags = IRRootDescriptorFlagNone },
							.ShaderVisibility = IRShaderVisibilityAll
						},
						{
							.ParameterType = IRRootParameterTypeDescriptorTable,
							.DescriptorTable = { .NumDescriptorRanges = arraysize(binding_resources), .pDescriptorRanges = binding_resources },
							.ShaderVisibility = IRShaderVisibilityAll
						},
						{
							.ParameterType = IRRootParameterTypeDescriptorTable,
							.DescriptorTable = { .NumDescriptorRanges = arraysize(binding_samplers), .pDescriptorRanges = binding_samplers },
							.ShaderVisibility = IRShaderVisibilityAll
						},
					};
					
					// Actually the static samplers don't work in Metal so I don't know why there is an API to describe them:
					//	Instead they will be put into the binding sampler table as a workaround (**)
					
					//static IRStaticSamplerDescriptor static_samplers[] =
					//{
					//	{ .ShaderRegister = 100, .RegisterSpace = 0, .Filter = IRFilterMinMagMipLinear, .AddressU = IRTextureAddressModeClamp, .AddressV = IRTextureAddressModeClamp, .AddressW = IRTextureAddressModeClamp, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 101, .RegisterSpace = 0, .Filter = IRFilterMinMagMipLinear, .AddressU = IRTextureAddressModeWrap, .AddressV = IRTextureAddressModeWrap, .AddressW = IRTextureAddressModeWrap, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 102, .RegisterSpace = 0, .Filter = IRFilterMinMagMipLinear, .AddressU = IRTextureAddressModeMirror, .AddressV = IRTextureAddressModeMirror, .AddressW = IRTextureAddressModeMirror, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//
					//	{ .ShaderRegister = 103, .RegisterSpace = 0, .Filter = IRFilterMinMagMipPoint, .AddressU = IRTextureAddressModeClamp, .AddressV = IRTextureAddressModeClamp, .AddressW = IRTextureAddressModeClamp, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 104, .RegisterSpace = 0, .Filter = IRFilterMinMagMipPoint, .AddressU = IRTextureAddressModeWrap, .AddressV = IRTextureAddressModeWrap, .AddressW = IRTextureAddressModeWrap, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 105, .RegisterSpace = 0, .Filter = IRFilterMinMagMipPoint, .AddressU = IRTextureAddressModeMirror, .AddressV = IRTextureAddressModeMirror, .AddressW = IRTextureAddressModeMirror, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//
					//	{ .ShaderRegister = 106, .RegisterSpace = 0, .Filter = IRFilterAnisotropic, .AddressU = IRTextureAddressModeClamp, .AddressV = IRTextureAddressModeClamp, .AddressW = IRTextureAddressModeClamp, .MipLODBias = 0, .MaxAnisotropy = 16, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 107, .RegisterSpace = 0, .Filter = IRFilterAnisotropic, .AddressU = IRTextureAddressModeWrap, .AddressV = IRTextureAddressModeWrap, .AddressW = IRTextureAddressModeWrap, .MipLODBias = 0, .MaxAnisotropy = 16, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//	{ .ShaderRegister = 108, .RegisterSpace = 0, .Filter = IRFilterAnisotropic, .AddressU = IRTextureAddressModeMirror, .AddressV = IRTextureAddressModeMirror, .AddressW = IRTextureAddressModeMirror, .MipLODBias = 0, .MaxAnisotropy = 16, .ComparisonFunc = IRComparisonFunctionNever, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = FLT_MAX, .ShaderVisibility = IRShaderVisibilityAll },
					//
					//	{ .ShaderRegister = 109, .RegisterSpace = 0, .Filter = IRFilterComparisonMinMagLinearMipPoint, .AddressU = IRTextureAddressModeClamp, .AddressV = IRTextureAddressModeClamp, .AddressW = IRTextureAddressModeClamp, .MipLODBias = 0, .MaxAnisotropy = 1, .ComparisonFunc = IRComparisonFunctionGreaterEqual, .BorderColor = IRStaticBorderColorOpaqueBlack, .MinLOD = 0, .MaxLOD = 0, .ShaderVisibility = IRShaderVisibilityAll },
					//};
					
					static const IRVersionedRootSignatureDescriptor desc = {
						.version = IRRootSignatureVersion_1_1,
						.desc_1_1.Flags = IRRootSignatureFlags(IRRootSignatureFlagAllowInputAssemblerInputLayout | IRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed | IRRootSignatureFlagSamplerHeapDirectlyIndexed),
						.desc_1_1.NumParameters = arraysize(root_parameters),
						.desc_1_1.pParameters = root_parameters,
						//.desc_1_1.NumStaticSamplers = arraysize(static_samplers),
						//.desc_1_1.pStaticSamplers = static_samplers,
					};
					static IRError* pRootSigError = nullptr;
					static IRRootSignature* pRootSig = IRRootSignatureCreateFromDescriptor(&desc, &pRootSigError);
					if (pRootSig == nullptr)
					{
						assert(0);
						IRErrorDestroy(pRootSigError);
					}
					
					IRCompiler* pCompiler = IRCompilerCreate();
					IRCompilerSetMinimumGPUFamily(pCompiler, IRGPUFamilyMetal3);
					IRCompilerSetCompatibilityFlags(pCompiler, IRCompatibilityFlags(IRCompatibilityFlagBoundsCheck | IRCompatibilityFlagPositionInvariance | IRCompatibilityFlagTextureMinLODClamp | IRCompatibilityFlagSamplerLODBias));
					IRCompilerSetValidationFlags(pCompiler, IRCompilerValidationFlagAll);
					IRCompilerSetEntryPointName(pCompiler, input.entrypoint.c_str());
					IRCompilerSetGlobalRootSignature(pCompiler, pRootSig);
					IRCompilerSetStageInGenerationMode(pCompiler, IRStageInCodeGenerationModeUseMetalVertexFetch);
					//IRCompilerEnableGeometryAndTessellationEmulation(pCompiler, true);
					IRObject* pDXIL = IRObjectCreateFromDXIL(output.shaderdata, output.shadersize, IRBytecodeOwnershipNone);
					IRError* pError = nullptr;
					IRObject* pOutIR = IRCompilerAllocCompileAndLink(pCompiler, NULL, pDXIL, &pError);
					if (pOutIR == nullptr)
					{
						assert(0);
						IRErrorDestroy(pError);
					}
					IRMetalLibBinary* pMetallib = IRMetalLibBinaryCreate();
					IRShaderStage irstage = {};
					switch (input.stage) {
						case ShaderStage::VS:
							irstage = IRShaderStageVertex;
							break;
						case ShaderStage::PS:
							irstage = IRShaderStageFragment;
							break;
						case ShaderStage::GS:
							irstage = IRShaderStageGeometry;
							break;
						case ShaderStage::HS:
							irstage = IRShaderStageHull;
							break;
						case ShaderStage::DS:
							irstage = IRShaderStageDomain;
							break;
						case ShaderStage::MS:
							irstage = IRShaderStageMesh;
							break;
						case ShaderStage::AS:
							irstage = IRShaderStageAmplification;
							break;
						case ShaderStage::CS:
							irstage = IRShaderStageCompute;
							break;
						default:
							assert(0);
							break;
					}
					IRObjectGetMetalLibBinary(pOutIR, irstage, pMetallib);
					size_t metallibSize = IRMetalLibGetBytecodeSize(pMetallib);
					auto internal_state = wi::allocator::make_shared<wi::vector<uint8_t>>(metallibSize); // lifetime storage of pointer
					IRMetalLibGetBytecode(pMetallib, internal_state->data());
					if (
						input.stage == ShaderStage::VS ||
						input.stage == ShaderStage::GS ||
						input.stage == ShaderStage::CS ||
						input.stage == ShaderStage::MS ||
						input.stage == ShaderStage::AS
						)
					{
						LINK_IR(IRShaderReflectionCreate)
						LINK_IR(IRObjectGetReflection)
						LINK_IR(IRShaderReflectionCopyVertexInfo)
						LINK_IR(IRShaderReflectionCopyGeometryInfo)
						LINK_IR(IRShaderReflectionCopyComputeInfo)
						LINK_IR(IRShaderReflectionCopyMeshInfo)
						LINK_IR(IRShaderReflectionCopyAmplificationInfo)
						LINK_IR(IRShaderReflectionDestroy)
						
						// Add the numthreads information to end of the shader:
						IRShaderReflection* reflection = IRShaderReflectionCreate();
						bool success = IRObjectGetReflection(pOutIR, irstage, reflection);
						assert(success);
						wi::graphics::GraphicsDevice_Metal::ShaderAdditionalData reflection_append = {};
						if (input.stage == ShaderStage::VS)
						{
							IRVersionedVSInfo vs_info = {};
							success = IRShaderReflectionCopyVertexInfo(reflection, IRReflectionVersion_1_0, &vs_info);
							assert(success);
							reflection_append.needs_draw_params = vs_info.info_1_0.needs_draw_params ? 1 : 0;
							reflection_append.vertex_output_size_in_bytes = vs_info.info_1_0.vertex_output_size_in_bytes;
						}
						if (input.stage == ShaderStage::GS)
						{
							IRVersionedGSInfo gs_info = {};
							success = IRShaderReflectionCopyGeometryInfo(reflection, IRReflectionVersion_1_0, &gs_info);
							assert(success);
							reflection_append.max_input_primitives_per_mesh_threadgroup = gs_info.info_1_0.max_input_primitives_per_mesh_threadgroup;
						}
						if (input.stage == ShaderStage::CS)
						{
							IRVersionedCSInfo cs_info = {};
							success = IRShaderReflectionCopyComputeInfo(reflection, IRReflectionVersion_1_0, &cs_info);
							assert(success);
							reflection_append.numthreads.width = cs_info.info_1_0.tg_size[0];
							reflection_append.numthreads.height = cs_info.info_1_0.tg_size[1];
							reflection_append.numthreads.depth = cs_info.info_1_0.tg_size[2];
						}
						else if (input.stage == ShaderStage::MS)
						{
							IRVersionedMSInfo ms_info = {};
							success = IRShaderReflectionCopyMeshInfo(reflection, IRReflectionVersion_1_0, &ms_info);
							assert(success);
							reflection_append.numthreads.width = ms_info.info_1_0.num_threads[0];
							reflection_append.numthreads.height = ms_info.info_1_0.num_threads[1];
							reflection_append.numthreads.depth = ms_info.info_1_0.num_threads[2];
						}
						else if (input.stage == ShaderStage::AS)
						{
							IRVersionedASInfo as_info = {};
							success = IRShaderReflectionCopyAmplificationInfo(reflection, IRReflectionVersion_1_0, &as_info);
							assert(success);
							reflection_append.numthreads.width = as_info.info_1_0.num_threads[0];
							reflection_append.numthreads.height = as_info.info_1_0.num_threads[1];
							reflection_append.numthreads.depth = as_info.info_1_0.num_threads[2];
						}
						internal_state->resize(internal_state->size() + sizeof(reflection_append));
						std::memcpy(internal_state->data() + internal_state->size() - sizeof(reflection_append), &reflection_append, sizeof(reflection_append));
						IRShaderReflectionDestroy(reflection);
					}
					output.internal_state = internal_state;
					output.shaderdata = internal_state->data();
					output.shadersize = internal_state->size();
					IRMetalLibBinaryDestroy(pMetallib);
					IRObjectDestroy(pDXIL);
					IRObjectDestroy(pOutIR);
					IRCompilerDestroy(pCompiler);
					//IRRootSignatureDestroy(pRootSig);
				}
			}
#endif // SHADERCOMPILER_APPLE_INCLUDED
			
		}

		if (input.format == ShaderFormat::HLSL6)
		{
			ComPtr<IDxcBlob> pHash = nullptr;
			hr = pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
			assert(SUCCEEDED(hr));
			if (pHash != nullptr)
			{
				DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
				for (int i = 0; i < _countof(pHashBuf->HashDigest); i++)
				{
					output.shaderhash.push_back(pHashBuf->HashDigest[i]);
				}
			}
		}
	}
#endif // SHADERCOMPILER_ENABLED_DXCOMPILER

#ifdef SHADERCOMPILER_ENABLED_D3DCOMPILER
	struct InternalState_D3DCompiler
	{
		using PFN_D3DCOMPILE = decltype(&D3DCompile);
		PFN_D3DCOMPILE D3DCompile = nullptr;

		InternalState_D3DCompiler()
		{
			if (D3DCompile != nullptr)
			{
				return; // already initialized
			}

			HMODULE d3dcompiler = wiLoadLibrary("d3dcompiler_47.dll");
			if (d3dcompiler != nullptr)
			{
				D3DCompile = (PFN_D3DCOMPILE)wiGetProcAddress(d3dcompiler, "D3DCompile");
				if (D3DCompile != nullptr)
				{
					wi::backlog::post("wi::shadercompiler: loaded d3dcompiler_47.dll");
				}
			}
		}
	};
	inline InternalState_D3DCompiler& d3d_compiler()
	{
		static InternalState_D3DCompiler internal_state;
		return internal_state;
	}

	void Compile_D3DCompiler(const CompilerInput& input, CompilerOutput& output)
	{
		if (d3d_compiler().D3DCompile == nullptr)
		{
			return;
		}

		if (input.minshadermodel > ShaderModel::SM_5_0)
		{
			output.error_message = "SHADERFORMAT_HLSL5 cannot support specified minshadermodel!";
			return;
		}

		wi::vector<uint8_t> shadersourcedata;
		if (!wi::helper::FileRead(input.shadersourcefilename, shadersourcedata))
		{
			return;
		}

		D3D_SHADER_MACRO defines[] = {
			{"HLSL5", "1"},
			{"DISABLE_WAVE_INTRINSICS", "1"},
			{NULL, NULL},
		};

		const char* target = nullptr;
		switch (input.stage)
		{
		default:
		case ShaderStage::MS:
		case ShaderStage::AS:
		case ShaderStage::LIB:
			// not applicable
			return;
		case ShaderStage::VS:
			target = "vs_5_0";
			break;
		case ShaderStage::HS:
			target = "hs_5_0";
			break;
		case ShaderStage::DS:
			target = "ds_5_0";
			break;
		case ShaderStage::GS:
			target = "gs_5_0";
			break;
		case ShaderStage::PS:
			target = "ps_5_0";
			break;
		case ShaderStage::CS:
			target = "cs_5_0";
			break;
		}

		struct IncludeHandler final : public ID3DInclude
		{
			const CompilerInput* input = nullptr;
			CompilerOutput* output = nullptr;
			wi::vector<wi::vector<uint8_t>> filedatas;

			HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
			{
				for (auto& x : input->include_directories)
				{
					std::string filename = x + pFileName;
					if (!wi::helper::FileExists(filename))
						continue;
					wi::vector<uint8_t>& filedata = filedatas.emplace_back();
					if (wi::helper::FileRead(filename, filedata))
					{
						output->dependencies.push_back(filename);
						*ppData = filedata.data();
						*pBytes = (UINT)filedata.size();
						return S_OK;
					}
				}
				return E_FAIL;
			}

			HRESULT Close(LPCVOID pData) override
			{
				return S_OK;
			}
		} includehandler;
		includehandler.input = &input;
		includehandler.output = &output;

		// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/d3dcompile-constants
		UINT Flags1 = 0;
		if (has_flag(input.flags, Flags::DISABLE_OPTIMIZATION))
		{
			Flags1 |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}


		ComPtr<ID3DBlob> code;
		ComPtr<ID3DBlob> errors;
		HRESULT hr = d3d_compiler().D3DCompile(
			shadersourcedata.data(),
			shadersourcedata.size(),
			input.shadersourcefilename.c_str(),
			defines,
			&includehandler, //D3D_COMPILE_STANDARD_FILE_INCLUDE,
			input.entrypoint.c_str(),
			target,
			Flags1,
			0,
			&code,
			&errors
		);

		if (errors)
		{
			output.error_message = (const char*)errors->GetBufferPointer();
		}

		if (SUCCEEDED(hr))
		{
			output.dependencies.push_back(input.shadersourcefilename);
			output.shaderdata = (const uint8_t*)code->GetBufferPointer();
			output.shadersize = code->GetBufferSize();

			// keep the blob alive == keep shader pointer valid!
			auto internal_state = wi::allocator::make_shared<ComPtr<ID3D10Blob>>();
			*internal_state = code;
			output.internal_state = internal_state;
		}
	}
#endif // SHADERCOMPILER_ENABLED_D3DCOMPILER

	void Compile(const CompilerInput& input, CompilerOutput& output)
	{
		output = CompilerOutput();

#ifdef SHADERCOMPILER_ENABLED
		switch (input.format)
		{
		default:
			break;

#ifdef SHADERCOMPILER_ENABLED_DXCOMPILER
		case ShaderFormat::HLSL6:
		case ShaderFormat::SPIRV:
		case ShaderFormat::HLSL6_XS:
		case ShaderFormat::METAL:
			Compile_DXCompiler(input, output);
			break;
#endif // SHADERCOMPILER_ENABLED_DXCOMPILER

#ifdef SHADERCOMPILER_ENABLED_D3DCOMPILER
		case ShaderFormat::HLSL5:
			Compile_D3DCompiler(input, output);
			break;
#endif // SHADERCOMPILER_ENABLED_D3DCOMPILER

#ifdef SHADERCOMPILER_PS5_INCLUDED
		case ShaderFormat::PS5:
			wi::shadercompiler::ps5::Compile(input, output);
			break;
#endif // SHADERCOMPILER_PS5_INCLUDED

		}
#endif // SHADERCOMPILER_ENABLED
	}

	constexpr const char* shadermetaextension = "wishadermeta";
	bool SaveShaderAndMetadata(const std::string& shaderfilename, const CompilerOutput& output)
	{
#ifdef SHADERCOMPILER_ENABLED
		wi::helper::DirectoryCreate(wi::helper::GetDirectoryFromPath(shaderfilename));

		wi::Archive dependencyLibrary(wi::helper::ReplaceExtension(shaderfilename, shadermetaextension), false);
		if (dependencyLibrary.IsOpen())
		{
			std::string rootdir = dependencyLibrary.GetSourceDirectory();
			wi::vector<std::string> dependencies = output.dependencies;
			for (auto& x : dependencies)
			{
				wi::helper::MakePathRelative(rootdir, x);
			}
			dependencyLibrary << dependencies;
		}

		if (wi::helper::FileWrite(shaderfilename, output.shaderdata, output.shadersize))
		{
			return true;
		}
#endif // SHADERCOMPILER_ENABLED

		return false;
	}
	bool IsShaderOutdated(const std::string& shaderfilename)
	{
#ifdef SHADERCOMPILER_ENABLED
		std::string filepath = shaderfilename;
		wi::helper::MakePathAbsolute(filepath);
		if (!wi::helper::FileExists(filepath))
		{
			return true; // no shader file = outdated shader, apps can attempt to rebuild it
		}
		std::string dependencylibrarypath = wi::helper::ReplaceExtension(shaderfilename, shadermetaextension);
		if (!wi::helper::FileExists(dependencylibrarypath))
		{
			return false; // no metadata file = no dependency, up to date (for example packaged builds)
		}

		const uint64_t tim = wi::helper::FileTimestamp(filepath);

		// Open the metadata silently: an incompatible (e.g. newer) cache must
		// not pop an error box from this worker thread - it just means the
		// cached shader is stale and should be rebuilt.
		wi::Archive dependencyLibrary(dependencylibrarypath, true, false);
		if (!dependencyLibrary.IsOpen())
		{
			return true; // unreadable/incompatible metadata = outdated, rebuild it
		}

		std::string rootdir = dependencyLibrary.GetSourceDirectory();
		wi::vector<std::string> dependencies;
		dependencyLibrary >> dependencies;

		for (auto& x : dependencies)
		{
			std::string dependencypath = rootdir + x;
			wi::helper::MakePathAbsolute(dependencypath);
			if (wi::helper::FileExists(dependencypath))
			{
				const uint64_t dep_tim = wi::helper::FileTimestamp(dependencypath);

				if (tim < dep_tim)
				{
					return true;
				}
			}
		}
#endif // SHADERCOMPILER_ENABLED

		return false;
	}

	namespace identity_detail
	{
		// SHA-256 (FIPS 180-4): published identities must hash identically on every platform.
		class Sha256
		{
		public:
			void Update(const void* data, size_t size)
			{
				const uint8_t* bytes = (const uint8_t*)data;
				bits += uint64_t(size) * 8;
				while (size > 0)
				{
					const size_t take = std::min(size, size_t(64) - used);
					std::memcpy(buffer + used, bytes, take);
					used += take;
					bytes += take;
					size -= take;
					if (used == 64)
					{
						Block(buffer);
						used = 0;
					}
				}
			}
			std::string FinishHex()
			{
				const uint64_t length = bits;
				const uint8_t pad = 0x80;
				Update(&pad, 1);
				const uint8_t zero = 0;
				while (used != 56)
				{
					Update(&zero, 1);
				}
				uint8_t tail[8];
				for (int i = 0; i < 8; ++i)
				{
					tail[i] = uint8_t(length >> (56 - 8 * i));
				}
				Update(tail, 8);
				static const char* digits = "0123456789abcdef";
				std::string hex;
				for (uint32_t word : h)
				{
					for (int shift = 28; shift >= 0; shift -= 4)
					{
						hex += digits[(word >> shift) & 0xF];
					}
				}
				return hex;
			}
		private:
			static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
			void Block(const uint8_t* p)
			{
				static const uint32_t k[64] = {
					0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
					0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
					0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
					0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
					0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
					0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
					0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
					0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
				};
				uint32_t w[64];
				for (int i = 0; i < 16; ++i)
				{
					w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) | (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
				}
				for (int i = 16; i < 64; ++i)
				{
					const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
					const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
					w[i] = w[i - 16] + s0 + w[i - 7] + s1;
				}
				uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
				for (int i = 0; i < 64; ++i)
				{
					const uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
					const uint32_t ch = (e & f) ^ (~e & g);
					const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
					const uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
					const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
					const uint32_t t2 = S0 + maj;
					hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
				}
				h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
			}
			uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
			uint8_t buffer[64] = {};
			size_t used = 0;
			uint64_t bits = 0;
		};

		struct FileFacts
		{
			std::uintmax_t size = 0;
			std::filesystem::file_time_type stamp;
			std::string digest;
			wi::vector<std::string> includes; // names as written, in order
		};
		std::mutex facts_locker;
		wi::unordered_map<std::string, FileFacts> facts;

		// Digest and #include names of a file, recomputed only when its size or write time changes.
		bool Facts(const std::filesystem::path& path, FileFacts& out)
		{
			std::error_code error;
			const auto size = std::filesystem::file_size(path, error);
			if (error)
				return false;
			const auto stamp = std::filesystem::last_write_time(path, error);
			if (error)
				return false;
			const std::string key = path.generic_string();
			{
				std::scoped_lock lock(facts_locker);
				auto found = facts.find(key);
				if (found != facts.end() && found->second.size == size && found->second.stamp == stamp)
				{
					out = found->second;
					return true;
				}
			}
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return false;
			std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			FileFacts result;
			result.size = size;
			result.stamp = stamp;
			Sha256 sha;
			sha.Update(content.data(), content.size());
			result.digest = sha.FinishHex();
			std::istringstream lines(content);
			std::string line;
			while (std::getline(lines, line))
			{
				size_t i = line.find_first_not_of(" \t");
				if (i == std::string::npos || line[i] != '#')
					continue;
				i = line.find_first_not_of(" \t", i + 1);
				if (i == std::string::npos || line.compare(i, 7, "include") != 0)
					continue;
				const size_t open = line.find_first_of("\"<", i + 7);
				if (open == std::string::npos)
					continue;
				const size_t close = line.find(line[open] == '"' ? '"' : '>', open + 1);
				if (close == std::string::npos)
					continue;
				result.includes.push_back(line.substr(open + 1, close - open - 1));
			}
			{
				std::scoped_lock lock(facts_locker);
				facts[key] = result;
			}
			out = std::move(result);
			return true;
		}

		std::string CompilerVersion(wi::graphics::ShaderFormat format)
		{
			switch (format)
			{
#ifdef SHADERCOMPILER_ENABLED_DXCOMPILER
			case ShaderFormat::HLSL6:
			case ShaderFormat::SPIRV:
				return dxc_compiler().version;
			case ShaderFormat::METAL:
				return dxc_compiler().version + " + metal shader converter";
#endif // SHADERCOMPILER_ENABLED_DXCOMPILER
			default:
				return "format " + std::to_string((int)format);
			}
		}

		// The shader model the compiler actually targets (mirrors Compile_DXCompiler): requests that
		// compile to the same profile must not produce different identities.
		ShaderModel EffectiveModel(const CompilerInput& input)
		{
			switch (input.format)
			{
			case ShaderFormat::HLSL6_XS:
				return ShaderModel::SM_6_2;
			case ShaderFormat::METAL:
				return std::max(input.minshadermodel, ShaderModel::SM_6_6);
			case ShaderFormat::HLSL6:
			case ShaderFormat::SPIRV:
				return std::max(input.minshadermodel, ShaderModel::SM_6_0);
			default:
				return input.minshadermodel;
			}
		}

		wi::unordered_map<std::string, std::string> manifest;
		std::mutex manifest_locker;

		std::string Trim(const std::string& text)
		{
			const size_t first = text.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
				return {};
			const size_t last = text.find_last_not_of(" \t\r\n");
			return text.substr(first, last - first + 1);
		}
	}

	std::string ComputeShaderIdentity(const CompilerInput& input)
	{
		namespace fs = std::filesystem;
		using namespace identity_detail;
		std::error_code error;
		const fs::path source = fs::weakly_canonical(fs::path(input.shadersourcefilename), error);
		const fs::path root = fs::weakly_canonical(input.include_directories.empty() ? source.parent_path() : fs::path(input.include_directories.front()), error);

		std::string text = "dmo-shader-identity v1\n";
		text += "format " + std::to_string((int)input.format) + "\n";
		text += "stage " + std::to_string((int)input.stage) + "\n";
		text += "entry " + input.entrypoint + "\n";
		text += "model " + std::to_string((int)EffectiveModel(input)) + "\n";
		text += "flags " + std::to_string((int)input.flags) + "\n";
		text += "compiler " + CompilerVersion(input.format) + "\n";
		wi::vector<std::string> defines = input.defines;
		std::sort(defines.begin(), defines.end());
		for (const auto& define : defines)
		{
			text += "define " + define + "\n";
		}

		// Depth-first include closure in include order; each file once.
		wi::unordered_set<std::string> visited;
		auto visit = [&](auto&& self, const fs::path& file) -> void {
			const std::string key = file.generic_string();
			if (!visited.insert(key).second)
				return;
			FileFacts fileFacts;
			const std::string relative = file.lexically_relative(root).generic_string();
			if (!Facts(file, fileFacts))
			{
				text += "missing " + relative + "\n";
				return;
			}
			text += "file " + relative + " " + fileFacts.digest + "\n";
			for (const auto& name : fileFacts.includes)
			{
				fs::path resolved;
				const fs::path candidate = file.parent_path() / name;
				if (fs::exists(candidate, error))
				{
					resolved = candidate;
				}
				else
				{
					for (const auto& directory : input.include_directories)
					{
						const fs::path option = fs::path(directory) / name;
						if (fs::exists(option, error))
						{
							resolved = option;
							break;
						}
					}
				}
				if (resolved.empty())
				{
					text += "unresolved " + name + "\n";
					continue;
				}
				self(self, fs::weakly_canonical(resolved, error));
			}
		};
		visit(visit, source);

		Sha256 sha;
		sha.Update(text.data(), text.size());
		return sha.FinishHex();
	}

	std::string Sha256Hex(const void* data, size_t size)
	{
		identity_detail::Sha256 sha;
		sha.Update(data, size);
		return sha.FinishHex();
	}

	std::string ReadShaderIdentity(const std::string& shaderfilename)
	{
		std::ifstream stream(wi::helper::ReplaceExtension(shaderfilename, "wishaderid"));
		if (!stream)
			return {};
		std::string identity;
		std::getline(stream, identity);
		return identity_detail::Trim(identity);
	}

	bool WriteShaderIdentity(const std::string& shaderfilename, const std::string& identity)
	{
		const std::string path = wi::helper::ReplaceExtension(shaderfilename, "wishaderid");
		const std::string text = identity + "\n";
		return wi::helper::FileWrite(path, (const uint8_t*)text.data(), text.size());
	}

	bool LoadShaderIdentityManifest(const std::string& manifestfilename)
	{
		std::ifstream stream(manifestfilename);
		if (!stream)
			return false;
		std::string line;
		if (!std::getline(stream, line) || identity_detail::Trim(line).rfind("dmo.shader-identity-manifest.v1", 0) != 0)
			return false;
		wi::unordered_map<std::string, std::string> entries;
		while (std::getline(stream, line))
		{
			line = identity_detail::Trim(line);
			if (line.empty())
				continue;
			const size_t space = line.find_last_of(' ');
			if (space == std::string::npos || space == 0)
				return false;
			const std::string identity = line.substr(space + 1);
			if (identity.size() != 64 || identity.find_first_not_of("0123456789abcdef") != std::string::npos)
				return false;
			entries[identity_detail::Trim(line.substr(0, space))] = identity;
		}
		std::scoped_lock lock(identity_detail::manifest_locker);
		identity_detail::manifest = std::move(entries);
		return true;
	}

	bool WriteShaderIdentityManifest(const std::string& manifestfilename, const wi::vector<std::pair<std::string, std::string>>& entries)
	{
		wi::vector<std::pair<std::string, std::string>> sorted = entries;
		std::sort(sorted.begin(), sorted.end());
		std::string text = "dmo.shader-identity-manifest.v1\n";
		for (const auto& [binary, identity] : sorted)
		{
			text += binary + " " + identity + "\n";
		}
		return wi::helper::FileWrite(manifestfilename, (const uint8_t*)text.data(), text.size());
	}

	void ClearShaderIdentityManifest()
	{
		std::scoped_lock lock(identity_detail::manifest_locker);
		identity_detail::manifest.clear();
	}

	std::string ExpectedShaderIdentity(const std::string& shaderbinaryname)
	{
		std::scoped_lock lock(identity_detail::manifest_locker);
		auto found = identity_detail::manifest.find(shaderbinaryname);
		return found == identity_detail::manifest.end() ? std::string() : found->second;
	}

	bool IsShaderOutdated(const std::string& shaderfilename, const std::string& expectedidentity)
	{
		if (expectedidentity.empty())
			return IsShaderOutdated(shaderfilename);
		std::string filepath = shaderfilename;
		wi::helper::MakePathAbsolute(filepath);
		if (!wi::helper::FileExists(filepath))
			return true;
		return ReadShaderIdentity(filepath) != expectedidentity;
	}

	std::mutex locker;
	wi::unordered_set<std::string> registered_shaders;
	void RegisterShader(const std::string& shaderfilename)
	{
#ifdef SHADERCOMPILER_ENABLED
		std::scoped_lock lock(locker);
		registered_shaders.insert(shaderfilename);
#endif // SHADERCOMPILER_ENABLED
	}
	size_t GetRegisteredShaderCount()
	{
		std::scoped_lock lock(locker);
		return registered_shaders.size();
	}
	bool CheckRegisteredShadersOutdated()
	{
#ifdef SHADERCOMPILER_ENABLED
		std::scoped_lock lock(locker);
		for (auto& x : registered_shaders)
		{
			if (IsShaderOutdated(x))
			{
				return true;
			}
		}
#endif // SHADERCOMPILER_ENABLED
		return false;
	}
}
