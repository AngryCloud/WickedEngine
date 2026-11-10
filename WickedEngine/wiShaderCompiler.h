#pragma once
#include "wiGraphics.h"
#include "wiVector.h"
#include "wiAllocator.h"

#include <string>

namespace wi::shadercompiler
{

	enum class Flags
	{
		NONE = 0,
		DISABLE_OPTIMIZATION = 1 << 0,
		STRIP_REFLECTION = 1 << 1,
		KEEP_DEBUG_INFORMATION = 1 << 2,
	};
	struct CompilerInput
	{
		Flags flags = Flags::NONE;
		wi::graphics::ShaderFormat format = wi::graphics::ShaderFormat::NONE;
		wi::graphics::ShaderStage stage = wi::graphics::ShaderStage::Count;
		// if the shader relies on a higher shader model feature, it must be declared here.
		//	But the compiler can also choose a higher one internally, if needed
		wi::graphics::ShaderModel minshadermodel = wi::graphics::ShaderModel::SM_5_0;
		std::string shadersourcefilename;
		std::string entrypoint = "main";
		wi::vector<std::string> include_directories;
		wi::vector<std::string> defines;
	};
	struct CompilerOutput
	{
		wi::allocator::shared_ptr<void> internal_state;
		constexpr bool IsValid() const { return internal_state.IsValid(); }

		const uint8_t* shaderdata = nullptr;
		size_t shadersize = 0;
		wi::vector<uint8_t> shaderhash;
		std::string error_message;
		wi::vector<std::string> dependencies;
	};
	void Compile(const CompilerInput& input, CompilerOutput& output);

	bool SaveShaderAndMetadata(const std::string& shaderfilename, const CompilerOutput& output);
	bool IsShaderOutdated(const std::string& shaderfilename);

	// Content identity of a compiled shader: a stable SHA-256 (hex) over everything that determines
	// it: target format, stage, entry point, minimum shader model, flags, sorted defines, compiler
	// version, and every file of the source's #include closure (by source-root-relative path and
	// content). The include closure is found by scanning #include lines, a superset of the real
	// dependencies, so an edit is never missed. File digests and include lists are cached per
	// process while a file's size and write time are unchanged.
	std::string ComputeShaderIdentity(const CompilerInput& input);
	// SHA-256 of a byte range as lowercase hex: the identity hash primitive.
	std::string Sha256Hex(const void* data, size_t size);
	// The identity recorded beside a binary (<binary>.wishaderid), or empty.
	std::string ReadShaderIdentity(const std::string& shaderfilename);
	bool WriteShaderIdentity(const std::string& shaderfilename, const std::string& identity);
	// A published identity manifest ("dmo.shader-identity-manifest.v1" header, then one
	// "<binary relative to the shader directory> <identity>" line per shader), as a shader patch
	// installs it. Loading replaces the current manifest.
	bool LoadShaderIdentityManifest(const std::string& manifestfilename);
	bool WriteShaderIdentityManifest(const std::string& manifestfilename, const wi::vector<std::pair<std::string, std::string>>& entries);
	void ClearShaderIdentityManifest();
	std::string ExpectedShaderIdentity(const std::string& shaderbinaryname); // empty when not listed
	// Outdated when the binary is missing or its recorded identity differs from the expected one.
	// An empty expected identity keeps the timestamp check above.
	bool IsShaderOutdated(const std::string& shaderfilename, const std::string& expectedidentity);

	void RegisterShader(const std::string& shaderfilename);
	size_t GetRegisteredShaderCount();
	bool CheckRegisteredShadersOutdated();
}

template<>
struct enable_bitmask_operators<wi::shadercompiler::Flags> : std::true_type {};
