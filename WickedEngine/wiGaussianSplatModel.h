#pragma once
#include "CommonInclude.h"
#include "wiGraphicsDevice.h"
#include "wiPrimitive.h"
#include "wiVector.h"
#include "wiMath.h"
#include "wiECS.h"
#include "wiScene_Decl.h"

namespace wi
{
	class Archive;

	// One gaussian splat asset:
	class GaussianSplatModel
	{
	public:
		enum FLAGS
		{
			EMPTY = 0,
			INSTANCE_ASSET = 1 << 0, // drawn only through GaussianSplatInstanceSet placements, never at its own transform
		};
		uint32_t _flags = EMPTY;

		wi::vector<XMFLOAT3> positions;
		wi::vector<XMFLOAT4> rotations;
		wi::vector<XMFLOAT3> scales;
		wi::vector<float> opacities;
		wi::vector<XMFLOAT3> f_dc;
		wi::vector<float> f_rest; // number of floats depends on SH degree (L1:9, L2:24, L3:45)

		// Below this are non-serialized attributes:
		wi::primitive::AABB aabb_rest; // aabb without transformation
		wi::primitive::AABB aabb; // aabb with transformation
		XMFLOAT4X4 transform = wi::math::IDENTITY_MATRIX;
		XMFLOAT4X4 transform_inverse = wi::math::IDENTITY_MATRIX;
		float maxScale = 1;
		float boundingRadius = 0; // centre of aabb_rest to its corner
		float maxSplatExtent = 0; // largest splat extent drawn (sqrt(8) standard deviations), for culling placements
		wi::graphics::GPUBuffer buffer;
		int subresource_splatBuffer = -1;
		int subresource_shBuffer = -1;

		constexpr void SetInstanceAsset(bool value) { if (value) { _flags |= INSTANCE_ASSET; } else { _flags &= ~INSTANCE_ASSET; } }
		constexpr bool IsInstanceAsset() const { return _flags & INSTANCE_ASSET; }

		size_t GetSplatCount() const { return positions.size(); };
		int GetSphericalHarmonicsDegree() const;
		static int GetSphericalHarmonicsCount(int degree); // view dependent coefficients per colour channel: 0, 3, 8 or 15
		size_t GetMemorySizeCPU() const;
		size_t GetMemorySizeGPU() const;

		void CreateRenderData();

		void Update(const XMFLOAT4X4& matrix);

		void Serialize(wi::Archive& archive, wi::ecs::EntitySerializer& seri);

		static void Initialize();
	};

	// One placement of a shared splat asset:
	struct GaussianSplatInstance
	{
		XMFLOAT4X4 transform = wi::math::IDENTITY_MATRIX; // world matrix
		uint32_t splatCount = ~0u; // draws the first splatCount splats (clamped to the asset, 0 draws nothing). Assets baked in a progressive order make every prefix a uniform subset
		float footprintScale = 1; // multiplies splat extents; sqrt(assetSplatCount / splatCount) keeps a reduced prefix covering the same area
		XMFLOAT4 tint = XMFLOAT4(1, 1, 1, 1); // linear colour multiplier (rgb) and opacity multiplier (a)
	};

	// Many placements of one shared GaussianSplatModel. The model entity should be an instance asset so it isn't also drawn by itself:
	class GaussianSplatInstanceSet
	{
	public:
		wi::ecs::Entity model = wi::ecs::INVALID_ENTITY;
		wi::vector<GaussianSplatInstance> instances;

		// Below this are non-serialized attributes, refreshed by Update():
		const GaussianSplatModel* asset = nullptr;
		wi::vector<XMFLOAT4> bounds; // per instance: world centre of the asset bounds (xyz) and the largest axis scale (w)
		wi::primitive::AABB aabb; // all placements, with splat extents at footprintScale 1

		uint32_t GetDrawCount(size_t index) const; // splats drawn for one instance
		wi::primitive::Sphere GetBoundingSphere(size_t index) const; // includes splat extents at the instance footprintScale

		void Update(const GaussianSplatModel* model);

		void Serialize(wi::Archive& archive, wi::ecs::EntitySerializer& seri);
	};

	// One draw submitted to GaussianSplatScene::UpdateGPU, valid for that call only:
	struct GaussianSplatDraw
	{
		const GaussianSplatModel* model = nullptr;
		const XMFLOAT4X4* transform = nullptr;
		uint32_t splatCount = 0;
		float footprintScale = 1;
		XMFLOAT4 tint = XMFLOAT4(1, 1, 1, 1);
	};

	// Managing sorting for all gaussian splat models:
	class GaussianSplatScene
	{
	public:
		size_t model_capacity = 0;
		size_t draw_capacity = 0;
		size_t splat_capacity = 0;
		wi::graphics::GPUBuffer modelBuffer;
		wi::graphics::GPUBuffer instanceBuffer;
		wi::graphics::GPUBuffer indirectBuffer;
		wi::graphics::GPUBuffer sortedIndexBuffer;
		wi::graphics::GPUBuffer distanceBuffer;
		wi::graphics::GPUBuffer splatLookupBuffer;

		constexpr bool IsValid() const { return splat_capacity > 0; }

		void MakeReservations(size_t model_count, size_t draw_count, size_t splat_count); // upper bounds for one UpdateGPU call
		void UpdateGPU(const GaussianSplatDraw* draws, size_t draw_count, wi::graphics::CommandList cmd, uint32_t camera_count = 1) const; // culling and sorting. Multiple cameras can be provided when rendering will be performed to multiple destinations. Zero draws clears the previous result
		void Draw(wi::graphics::CommandList cmd, uint32_t camera_count = 1) const; // will be drawn with culling and sorting based on previous call to UpdateGPU
		void Clear();
	};
}
