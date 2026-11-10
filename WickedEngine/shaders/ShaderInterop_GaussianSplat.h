#ifndef WI_SHADERINTEROP_GAUSSIAN_SPLAT_H
#define WI_SHADERINTEROP_GAUSSIAN_SPLAT_H
#include "ShaderInterop.h"
#include "ShaderInterop_Renderer.h"

struct GaussianSplat
{
	uint2 position_radius;				// unorm16x3 | half
	uint2 color;						// half4
	uint2 cov3D_M11_M12_M13;			// half3
	uint2 cov3D_M22_M23_M33;			// half3
};
// One splat asset, shared by every placement that draws it:
struct ShaderGaussianSplatModel
{
	float3 aabb_min;					// for position remapping from unorm16x3 to float3
	int sphericalHarmonicsDegree;		// 0, 1, 2 or 3
	float3 aabb_max;					// for position remapping from unorm16x3 to float3
	int sphericalHarmonicsCount;		// element count of shBuffer per splat
	int descriptor_splatBuffer;			// pointer to StructuredBuffer<GaussianSplat> (one element per splat)
	int descriptor_shBuffer;			// pointer to StructuredBuffer<uint2> (sphericalHarmonicsCount element count per splat), -1 without view dependent colour
	uint splatCount;
	uint padding0;
};

// One placement of an asset. Placements own consecutive ranges of the culling dispatch, starting at splat_offset:
struct ShaderGaussianSplatInstance
{
	ShaderTransform transform;
	uint model_index;
	uint splat_offset;					// prefix sum of splat_count over the preceding placements
	uint splat_count;					// draws splats [0, splat_count) of the asset
	float footprint_scale;				// multiplies splat extents
	uint2 tint;							// half4: linear colour multiplier (rgb) and opacity multiplier (a)
	float cull_scale;					// largest axis scale * sqrt(8) * footprint_scale: turns a splat sigma into its culling radius
	uint padding0;
};

static const uint GAUSSIAN_COMPUTE_THREADSIZE = 64;

#endif // WI_SHADERINTEROP_GAUSSIAN_SPLAT_H
