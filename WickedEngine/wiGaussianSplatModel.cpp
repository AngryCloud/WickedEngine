#include "wiGaussianSplatModel.h"
#include "shaders/ShaderInterop_GaussianSplat.h"
#include "wiRenderer.h"
#include "wiBacklog.h"
#include "wiTimer.h"
#include "wiEventHandler.h"
#include "wiGPUSortLib.h"
#include "wiScene_Components.h"
#include "wiProfiler.h"

using namespace wi::math;
using namespace wi::graphics;
using namespace wi::primitive;

namespace wi
{
	static Shader computeShader;
	static Shader computeShader_indirect;
	static Shader vertexShader;
	static Shader pixelShader;
	static BlendState blendState;
	static RasterizerState rasterizerState;
	static DepthStencilState depthStencilState;
	static PipelineState pipelineState;

	void GaussianSplatModel::CreateRenderData()
	{
		GraphicsDevice* device = GetDevice();

		aabb_rest = AABB();
		float maxLogScale = -std::numeric_limits<float>::infinity();
		for (size_t splatIdx = 0; splatIdx < positions.size(); ++splatIdx)
		{
			aabb_rest.AddPoint(positions[splatIdx]);
			maxLogScale = std::max(maxLogScale, std::max(scales[splatIdx].x, std::max(scales[splatIdx].y, scales[splatIdx].z)));
		}
		boundingRadius = positions.empty() ? 0.0f : aabb_rest.getRadius();
		maxSplatExtent = positions.empty() ? 0.0f : std::exp(maxLogScale) * std::sqrt(8.0f);

		const uint32_t sphericalHarmonicsCoefficientsPerChannel = positions.empty() ? 0 : uint32_t(f_rest.size() / positions.size() / 3);
		const int sphericalHarmonicsCount = GetSphericalHarmonicsCount(GetSphericalHarmonicsDegree());

		GPUBufferDesc desc;
		desc.bind_flags = BindFlag::SHADER_RESOURCE;
		desc.misc_flags = ResourceMiscFlag::TYPED_FORMAT_CASTING | ResourceMiscFlag::NO_DEFAULT_DESCRIPTORS;

		const uint64_t alignment = device->GetMinOffsetAlignment(&desc);
		desc.size =
			align(uint64_t(positions.size() * sizeof(GaussianSplat)), alignment) +
			align(uint64_t(positions.size() * sphericalHarmonicsCount * sizeof(XMHALF4)), alignment);

		const uint64_t sh_aligned_offset = align(uint64_t(positions.size() * sizeof(GaussianSplat)), alignment);

		auto fill_gpu = [&](void* dest) {
			GaussianSplat* splat_dest = (GaussianSplat*)dest;
			XMHALF4* sh_dest = (XMHALF4*)((uint8_t*)dest + sh_aligned_offset);
			for (size_t splatIdx = 0; splatIdx < positions.size(); ++splatIdx)
			{
				GaussianSplat splat = {};

				// position remap and quantize to 16 bit UNORM:
				const XMFLOAT3 pos = wi::math::InverseLerp(aabb_rest._min, aabb_rest._max, positions[splatIdx]);
				splat.position_radius.x = uint32_t(saturate(pos.x) * 65535.0f);
				splat.position_radius.x |= uint32_t(saturate(pos.y) * 65535.0f) << 16u;
				splat.position_radius.y = uint32_t(saturate(pos.z) * 65535.0f);

				const XMFLOAT3 scale = XMFLOAT3(std::exp(scales[splatIdx].x), std::exp(scales[splatIdx].y), std::exp(scales[splatIdx].z));
				const float radius = std::max(scale.x, std::max(scale.y, scale.z)); // culling
				splat.position_radius.y |= uint32_t(XMConvertFloatToHalf(radius)) << 16u; // radius in half precision

				// f_dc is L0 spherical harmonics (not view dependent), so it's converted to rgb color here
				// https://github.com/nvpro-samples/vk_gaussian_splatting/blob/main/src/splat_set_vk.cpp
				//	I also remove SRGB curve here with pow(rgb, 2.2)
				static constexpr float SH_C0 = 0.28209479177387814f;
				float4 color;
				color.x = std::pow(saturate(0.5f + SH_C0 * f_dc[splatIdx].x), 2.2f);
				color.y = std::pow(saturate(0.5f + SH_C0 * f_dc[splatIdx].y), 2.2f);
				color.z = std::pow(saturate(0.5f + SH_C0 * f_dc[splatIdx].z), 2.2f);
				color.w = saturate(1.0f / (1.0f + std::exp(-opacities[splatIdx])));
				splat.color = pack_half4(color);

				// covariance from: https://github.com/nvpro-samples/vk_gaussian_splatting/blob/main/src/splat_set_vk.cpp
				//	changed from glm to DirectXMath (column->row major, matrix mul order changed)
				const XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
				const XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(XMLoadFloat4(&rotations[splatIdx]));
				const XMMATRIX covarianceMatrix = XMMatrixMultiply(scaleMatrix, rotationMatrix);
				const XMMATRIX transformedCovarianceMatrix = XMMatrixMultiply(XMMatrixTranspose(covarianceMatrix), covarianceMatrix);
				XMFLOAT3X3 transformedCovariance;
				XMStoreFloat3x3(&transformedCovariance, transformedCovarianceMatrix);
				float3 cov3D_M11_M12_M13;
				float3 cov3D_M22_M23_M33;
				cov3D_M11_M12_M13.x = transformedCovariance._11;
				cov3D_M11_M12_M13.y = transformedCovariance._12;
				cov3D_M11_M12_M13.z = transformedCovariance._13;
				cov3D_M22_M23_M33.x = transformedCovariance._22;
				cov3D_M22_M23_M33.y = transformedCovariance._23;
				cov3D_M22_M23_M33.z = transformedCovariance._33;
				splat.cov3D_M11_M12_M13 = pack_half3(cov3D_M11_M12_M13);
				splat.cov3D_M22_M23_M33 = pack_half3(cov3D_M22_M23_M33);

				std::memcpy(splat_dest + splatIdx, &splat, sizeof(splat)); // memcpy into uncached


				// View dependent SH data is deinterleaved, now I interleave it into 16x (rgb) vectors per splat.
				//	Only the coefficients of the supported degree are written, so lower degrees stay inside their own range:
				HALF* dst = (HALF*)(sh_dest + sphericalHarmonicsCount * splatIdx);
				const size_t srcBase = size_t(sphericalHarmonicsCoefficientsPerChannel) * 3 * splatIdx;
				for (int i = 0; i < sphericalHarmonicsCount; i++)
				{
					for (int rgb = 0; rgb < 3; rgb++)
					{
						dst[i * 4 + rgb] = XMConvertFloatToHalf(f_rest[srcBase + sphericalHarmonicsCoefficientsPerChannel * rgb + i]);
					}
					dst[i * 4 + 3] = 0; // 4th component wasted
				}
			}
		};

		bool success = device->CreateBuffer2(&desc, fill_gpu, &buffer);
		assert(success);
		device->SetName(&buffer, "GaussianSplatModel::buffer");

		static constexpr uint32_t splat_stride = sizeof(GaussianSplat);
		subresource_splatBuffer = device->CreateSubresource(&buffer, SubresourceType::SRV, 0, positions.size() * sizeof(GaussianSplat), nullptr, &splat_stride);

		subresource_shBuffer = -1;
		if (sphericalHarmonicsCount > 0)
		{
			static constexpr uint32_t sh_stride = sizeof(XMHALF4); // could be formatted buffer, but that has stricter size limitation on Mac OS
			subresource_shBuffer = device->CreateSubresource(&buffer, SubresourceType::SRV, sh_aligned_offset, positions.size() * sphericalHarmonicsCount * sizeof(XMHALF4), nullptr, &sh_stride);
		}
	}

	void GaussianSplatModel::Update(const XMFLOAT4X4& matrix)
	{
		const XMMATRIX W = XMLoadFloat4x4(&matrix);
		aabb = aabb_rest.transform(W);

		static const float sqrt8 = std::sqrt(8.0f);
		XMVECTOR scale = XMVectorSet(1, 1, 1, 1);
		scale = XMVector3TransformNormal(scale, W);
		maxScale = std::max(XMVectorGetX(scale), std::max(XMVectorGetY(scale), XMVectorGetZ(scale))) * sqrt8;

		XMStoreFloat4x4(&transform, W);
		XMStoreFloat4x4(&transform_inverse, XMMatrixInverse(nullptr, W));
	}

	void GaussianSplatModel::Serialize(wi::Archive& archive, wi::ecs::EntitySerializer& seri)
	{
		if (archive.IsReadMode())
		{
			if (seri.GetVersion() >= 1)
			{
				archive >> _flags;
			}
			archive >> positions;
			archive >> rotations;
			archive >> scales;
			archive >> opacities;
			archive >> f_dc;
			archive >> f_rest;

			wi::jobsystem::Execute(seri.ctx, [this](wi::jobsystem::JobArgs args) {
				CreateRenderData();
			});
		}
		else
		{
			if (seri.GetVersion() >= 1)
			{
				archive << _flags;
			}
			archive << positions;
			archive << rotations;
			archive << scales;
			archive << opacities;
			archive << f_dc;
			archive << f_rest;
		}
	}

	void GaussianSplatModel::Initialize()
	{
		wi::Timer timer;

		static auto LoadShaders = []{
			wi::renderer::LoadShader(ShaderStage::CS, computeShader, "gaussian_splatCS.cso");
			wi::renderer::LoadShader(ShaderStage::CS, computeShader_indirect, "gaussian_splat_indirectCS.cso");
			wi::renderer::LoadShader(ShaderStage::VS, vertexShader, "gaussian_splatVS.cso");
			wi::renderer::LoadShader(ShaderStage::PS, pixelShader, "gaussian_splatPS.cso");
		};
		static wi::eventhandler::Handle handle = wi::eventhandler::Subscribe(wi::eventhandler::EVENT_RELOAD_SHADERS, [](uint64_t userdata) { LoadShaders(); });
		LoadShaders();

		GraphicsDevice* device = GetDevice();

		depthStencilState.depth_enable = true;
		depthStencilState.depth_write_mask = DepthWriteMask::ZERO;
		depthStencilState.depth_func = ComparisonFunc::GREATER;

		rasterizerState.cull_mode = CullMode::BACK;
		rasterizerState.fill_mode = FillMode::SOLID;
		rasterizerState.depth_clip_enable = true;

		blendState.render_target[0].blend_enable = true;
		blendState.render_target[0].src_blend = Blend::ONE;
		blendState.render_target[0].dest_blend = Blend::INV_SRC_ALPHA;
		blendState.render_target[0].blend_op = BlendOp::ADD;
		blendState.render_target[0].src_blend_alpha = Blend::ONE;
		blendState.render_target[0].dest_blend_alpha = Blend::INV_SRC_ALPHA;
		blendState.render_target[0].blend_op_alpha = BlendOp::ADD;
		blendState.render_target[0].render_target_write_mask = ColorWrite::ENABLE_ALL;

		PipelineStateDesc desc;
		desc.vs = &vertexShader;
		desc.ps = &pixelShader;
		desc.rs = &rasterizerState;
		desc.bs = &blendState;
		desc.dss = &depthStencilState;
		desc.pt = PrimitiveTopology::TRIANGLESTRIP;
		bool success = device->CreatePipelineState(&desc, &pipelineState);
		assert(success);

		wilog("wi::GaussianSplatModel Initialized (%d ms)", (int)std::round(timer.elapsed()));
	}

	int GaussianSplatModel::GetSphericalHarmonicsDegree() const
	{
		if (positions.empty())
			return 0;
		const uint32_t sphericalHarmonicsCoefficientsPerChannel = uint32_t(f_rest.size() / positions.size() / 3);
		int sphericalHarmonicsDegree = 0;
		if (sphericalHarmonicsCoefficientsPerChannel >= 3)
		{
			sphericalHarmonicsDegree = 1;
		}
		if (sphericalHarmonicsCoefficientsPerChannel >= 8)
		{
			sphericalHarmonicsDegree = 2;
		}
		if (sphericalHarmonicsCoefficientsPerChannel == 15)
		{
			sphericalHarmonicsDegree = 3;
		}
		return sphericalHarmonicsDegree;
	}
	int GaussianSplatModel::GetSphericalHarmonicsCount(int degree)
	{
		static constexpr int counts[] = { 0, 3, 8, 15 };
		return counts[std::clamp(degree, 0, 3)];
	}
	size_t GaussianSplatModel::GetMemorySizeCPU() const
	{
		size_t ret = 0;
		ret += positions.size() * sizeof(XMFLOAT3);
		ret += rotations.size() * sizeof(XMFLOAT4);
		ret += scales.size() * sizeof(XMFLOAT3);
		ret += opacities.size() * sizeof(float);
		ret += f_dc.size() * sizeof(XMFLOAT3);
		ret += f_rest.size() * sizeof(float);
		return ret;
	}
	size_t GaussianSplatModel::GetMemorySizeGPU() const
	{
		return buffer.desc.size;
	}

	uint32_t GaussianSplatInstanceSet::GetDrawCount(size_t index) const
	{
		if (asset == nullptr)
			return 0;
		return std::min(instances[index].splatCount, uint32_t(asset->GetSplatCount()));
	}

	Sphere GaussianSplatInstanceSet::GetBoundingSphere(size_t index) const
	{
		const XMFLOAT4& bound = bounds[index];
		const float footprint = std::max(instances[index].footprintScale, 0.0f);
		return Sphere(XMFLOAT3(bound.x, bound.y, bound.z), bound.w * (asset->boundingRadius + asset->maxSplatExtent * footprint));
	}

	void GaussianSplatInstanceSet::Update(const GaussianSplatModel* model)
	{
		asset = model;
		aabb = AABB();
		bounds.resize(instances.size());
		if (asset == nullptr)
			return;
		const XMFLOAT3 restCenter = asset->aabb_rest.getCenter();
		const XMVECTOR center = XMLoadFloat3(&restCenter);
		for (size_t i = 0; i < instances.size(); ++i)
		{
			const float radius = asset->boundingRadius + asset->maxSplatExtent * std::max(instances[i].footprintScale, 1.0f);
			const XMMATRIX W = XMLoadFloat4x4(&instances[i].transform);
			const float axisScale = std::max(XMVectorGetX(XMVector3Length(W.r[0])), std::max(XMVectorGetX(XMVector3Length(W.r[1])), XMVectorGetX(XMVector3Length(W.r[2]))));
			XMFLOAT3 worldCenter;
			XMStoreFloat3(&worldCenter, XMVector3Transform(center, W));
			bounds[i] = XMFLOAT4(worldCenter.x, worldCenter.y, worldCenter.z, axisScale);
			const float extent = axisScale * radius;
			aabb = AABB::Merge(aabb, AABB(XMFLOAT3(worldCenter.x - extent, worldCenter.y - extent, worldCenter.z - extent), XMFLOAT3(worldCenter.x + extent, worldCenter.y + extent, worldCenter.z + extent)));
		}
	}

	void GaussianSplatInstanceSet::Serialize(wi::Archive& archive, wi::ecs::EntitySerializer& seri)
	{
		wi::ecs::SerializeEntity(archive, model, seri);
		if (archive.IsReadMode())
		{
			size_t count = 0;
			archive >> count;
			instances.resize(count);
			for (GaussianSplatInstance& instance : instances)
			{
				archive >> instance.transform;
				archive >> instance.splatCount;
				archive >> instance.footprintScale;
				archive >> instance.tint;
			}
		}
		else
		{
			archive << instances.size();
			for (const GaussianSplatInstance& instance : instances)
			{
				archive << instance.transform;
				archive << instance.splatCount;
				archive << instance.footprintScale;
				archive << instance.tint;
			}
		}
	}

	void GaussianSplatScene::MakeReservations(size_t model_count, size_t draw_count, size_t splat_count)
	{
		GraphicsDevice* device = GetDevice();

		if (!indirectBuffer.IsValid())
		{
			GPUBufferDesc desc;
			desc.stride = sizeof(IndirectDrawArgsInstanced);
			desc.size = desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED | ResourceMiscFlag::INDIRECT_ARGS;
			bool success = device->CreateBuffer(&desc, nullptr, &indirectBuffer);
			assert(success);
			device->SetName(&indirectBuffer, "GaussianSplatScene::indirectBuffer");
		}

		if (splat_count > splat_capacity)
		{
			splat_capacity = splat_count;

			GPUBufferDesc desc;
			desc.stride = sizeof(uint32_t);
			desc.size = splat_capacity * desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			bool success = device->CreateBuffer(&desc, nullptr, &sortedIndexBuffer);
			assert(success);
			device->SetName(&sortedIndexBuffer, "GaussianSplatScene::sortedIndexBuffer");

			desc.stride = sizeof(uint32_t);
			desc.size = splat_capacity * desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			success = device->CreateBuffer(&desc, nullptr, &distanceBuffer);
			assert(success);
			device->SetName(&distanceBuffer, "GaussianSplatScene::distanceBuffer");

			desc.stride = sizeof(uint2);
			desc.size = splat_capacity * desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			success = device->CreateBuffer(&desc, nullptr, &splatLookupBuffer);
			assert(success);
			device->SetName(&splatLookupBuffer, "GaussianSplatScene::splatLookupBuffer");
		}

		if (model_count > model_capacity)
		{
			model_capacity = model_count;

			GPUBufferDesc desc;
			desc.stride = sizeof(ShaderGaussianSplatModel);
			desc.size = model_capacity * desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			bool success = device->CreateBuffer(&desc, nullptr, &modelBuffer);
			assert(success);
			device->SetName(&modelBuffer, "GaussianSplatScene::modelBuffer");
		}

		if (draw_count > draw_capacity)
		{
			draw_capacity = draw_count;

			GPUBufferDesc desc;
			desc.stride = sizeof(ShaderGaussianSplatInstance);
			desc.size = draw_capacity * desc.stride;
			desc.bind_flags = BindFlag::SHADER_RESOURCE;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			bool success = device->CreateBuffer(&desc, nullptr, &instanceBuffer);
			assert(success);
			device->SetName(&instanceBuffer, "GaussianSplatScene::instanceBuffer");
		}
	}

	void GaussianSplatScene::UpdateGPU(const GaussianSplatDraw* draws, size_t draw_count, CommandList cmd, uint32_t camera_count) const
	{
		ScopedGPUProfiling("Gaussian splat culling and sorting", cmd);
		GraphicsDevice* device = GetDevice();
		device->EventBegin("Gaussian Splat Update", cmd);

		// Distinct assets and the placements that draw them. Placements own consecutive culling slots:
		thread_local wi::vector<const GaussianSplatModel*> models;
		thread_local wi::vector<ShaderGaussianSplatInstance> instances;
		models.clear();
		instances.clear();
		uint32_t slot_count = 0;
		static const float sqrt8 = std::sqrt(8.0f);
		for (size_t draw_index = 0; draw_index < draw_count && instances.size() < draw_capacity; ++draw_index)
		{
			const GaussianSplatDraw& draw = draws[draw_index];
			const uint32_t count = std::min(draw.splatCount, uint32_t(draw.model->GetSplatCount()));
			if (count == 0 || size_t(slot_count) + count > splat_capacity)
				continue;
			uint32_t model_index = 0;
			while (model_index < models.size() && models[model_index] != draw.model)
			{
				model_index++;
			}
			if (model_index == models.size())
			{
				if (models.size() >= model_capacity)
					continue;
				models.push_back(draw.model);
			}

			const XMMATRIX W = XMLoadFloat4x4(draw.transform);
			const float axisScale = std::max(XMVectorGetX(XMVector3Length(W.r[0])), std::max(XMVectorGetX(XMVector3Length(W.r[1])), XMVectorGetX(XMVector3Length(W.r[2]))));
			ShaderGaussianSplatInstance& instance = instances.emplace_back();
			instance = {};
			instance.transform.Create(*draw.transform);
			instance.model_index = model_index;
			instance.splat_offset = slot_count;
			instance.splat_count = count;
			instance.footprint_scale = std::max(draw.footprintScale, 0.0f);
			instance.tint = pack_half4(draw.tint);
			instance.cull_scale = axisScale * sqrt8 * instance.footprint_scale;
			slot_count += count;
		}

		IndirectDrawArgsInstanced args = {};
		args.VertexCountPerInstance = 4;
		args.InstanceCount = 0; // shader atomic dest
		args.StartVertexLocation = 0;
		args.StartInstanceLocation = 0;
		device->UpdateBuffer(&indirectBuffer, &args, cmd);

		if (instances.empty())
		{
			// Nothing drawn this time; the cleared arguments keep an earlier result from being drawn again:
			device->Barrier(GPUBarrier::Buffer(&indirectBuffer, ResourceState::COPY_DST, ResourceState::INDIRECT_ARGUMENT), cmd);
			device->EventEnd(cmd);
			return;
		}

		const size_t shmodeldatasize = sizeof(ShaderGaussianSplatModel) * models.size();
		auto model_alloc = device->AllocateGPU(shmodeldatasize, cmd);
		ShaderGaussianSplatModel* dest = (ShaderGaussianSplatModel*)model_alloc.data;
		for (size_t model_index = 0; model_index < models.size(); ++model_index)
		{
			const GaussianSplatModel& model = *models[model_index];
			ShaderGaussianSplatModel shmodel = {};
			shmodel.descriptor_splatBuffer = device->GetDescriptorIndex(&model.buffer, SubresourceType::SRV, model.subresource_splatBuffer);
			shmodel.descriptor_shBuffer = model.subresource_shBuffer < 0 ? -1 : device->GetDescriptorIndex(&model.buffer, SubresourceType::SRV, model.subresource_shBuffer);
			shmodel.aabb_min = model.aabb_rest._min;
			shmodel.aabb_max = model.aabb_rest._max;
			shmodel.sphericalHarmonicsDegree = model.subresource_shBuffer < 0 ? 0 : model.GetSphericalHarmonicsDegree();
			shmodel.sphericalHarmonicsCount = GaussianSplatModel::GetSphericalHarmonicsCount(shmodel.sphericalHarmonicsDegree);
			shmodel.splatCount = uint32_t(model.GetSplatCount());
			std::memcpy(dest + model_index, &shmodel, sizeof(shmodel)); // memcpy into uncached
		}
		device->CopyBuffer(&modelBuffer, 0, &model_alloc.buffer, model_alloc.offset, shmodeldatasize, cmd);

		const size_t shinstancedatasize = sizeof(ShaderGaussianSplatInstance) * instances.size();
		auto instance_alloc = device->AllocateGPU(shinstancedatasize, cmd);
		std::memcpy(instance_alloc.data, instances.data(), shinstancedatasize); // memcpy into uncached
		device->CopyBuffer(&instanceBuffer, 0, &instance_alloc.buffer, instance_alloc.offset, shinstancedatasize, cmd);

		{
			GPUBarrier barriers[] = {
				GPUBarrier::Buffer(&modelBuffer, ResourceState::COPY_DST, ResourceState::SHADER_RESOURCE),
				GPUBarrier::Buffer(&instanceBuffer, ResourceState::COPY_DST, ResourceState::SHADER_RESOURCE),
				GPUBarrier::Buffer(&indirectBuffer, ResourceState::COPY_DST, ResourceState::UNORDERED_ACCESS),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		// Every drawn splat of every placement is culled into the global buffer for sorting:
		{
			device->BindComputeShader(&computeShader, cmd);
			device->BindResource(&modelBuffer, 0, cmd);
			device->BindResource(&instanceBuffer, 1, cmd);
			device->BindUAV(&indirectBuffer, 0, cmd);
			device->BindUAV(&sortedIndexBuffer, 1, cmd);
			device->BindUAV(&distanceBuffer, 2, cmd);
			device->BindUAV(&splatLookupBuffer, 3, cmd);

			struct Push
			{
				uint instance_count;
				uint camera_count;
				uint dispatch_offset;
				uint slot_count;
			} push = {};
			push.instance_count = (uint)instances.size();
			push.camera_count = camera_count;
			push.slot_count = slot_count;

			// Some GPU can't exceed dispatch group count of 65535 in single dimension (DX12 validation), so I do multiple dispatches with max 65535 group count each:
			int remaining_threadgroups = ((int)slot_count + GAUSSIAN_COMPUTE_THREADSIZE - 1) / GAUSSIAN_COMPUTE_THREADSIZE;
			uint32_t group_offset = 0;
			while (remaining_threadgroups > 0)
			{
				push.dispatch_offset = group_offset * GAUSSIAN_COMPUTE_THREADSIZE;
				device->PushConstants(&push, sizeof(push), cmd);
				const uint32_t threadgroups = (uint32_t)std::min(remaining_threadgroups, 65535);
				device->Dispatch(threadgroups, 1, 1, cmd);
				remaining_threadgroups -= threadgroups;
				group_offset += threadgroups;
			}
		}

		{
			GPUBarrier barriers[] = {
				GPUBarrier::Memory(&sortedIndexBuffer),
				GPUBarrier::Memory(&distanceBuffer),
				GPUBarrier::Buffer(&indirectBuffer, ResourceState::UNORDERED_ACCESS, ResourceState::SHADER_RESOURCE),
				GPUBarrier::Buffer(&splatLookupBuffer, ResourceState::UNORDERED_ACCESS, ResourceState::SHADER_RESOURCE),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		// Sorting is done globally for buffer containing all models:
		wi::gpusortlib::Sort(slot_count, distanceBuffer, indirectBuffer, offsetof(IndirectDrawArgsInstanced, InstanceCount), sortedIndexBuffer, cmd);

		{
			GPUBarrier barriers[] = {
				GPUBarrier::Buffer(&sortedIndexBuffer, ResourceState::UNORDERED_ACCESS, ResourceState::SHADER_RESOURCE),
				GPUBarrier::Buffer(&distanceBuffer, ResourceState::UNORDERED_ACCESS, ResourceState::SHADER_RESOURCE),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		if (camera_count > 1)
		{
			// InstanceCount multiplied by cameraCount on GPU after sorting:
			device->BindComputeShader(&computeShader_indirect, cmd);
			device->BindUAV(&indirectBuffer, 0, cmd);
			device->PushConstants(&camera_count, sizeof(camera_count), cmd);
			device->Barrier(GPUBarrier::Buffer(&indirectBuffer, ResourceState::SHADER_RESOURCE, ResourceState::UNORDERED_ACCESS), cmd);
			device->Dispatch(1, 1, 1, cmd);
			device->Barrier(GPUBarrier::Buffer(&indirectBuffer, ResourceState::UNORDERED_ACCESS, ResourceState::INDIRECT_ARGUMENT), cmd);
		}
		else
		{
			device->Barrier(GPUBarrier::Buffer(&indirectBuffer, ResourceState::SHADER_RESOURCE, ResourceState::INDIRECT_ARGUMENT), cmd);
		}

		device->EventEnd(cmd);
	}

	void GaussianSplatScene::Draw(CommandList cmd, uint32_t camera_count) const
	{
		ScopedGPUProfiling("Gaussian splat drawing", cmd);
		GraphicsDevice* device = GetDevice();
		device->EventBegin("Gaussian Splat Render", cmd);
		device->BindPipelineState(&pipelineState, cmd);
		device->BindResource(&sortedIndexBuffer, 0, cmd);
		device->BindResource(&splatLookupBuffer, 1, cmd);
		device->BindResource(&modelBuffer, 2, cmd);
		device->BindResource(&instanceBuffer, 3, cmd);
		device->PushConstants(&camera_count, sizeof(camera_count), cmd);
		device->DrawInstancedIndirect(&indirectBuffer, 0, cmd);
		device->EventEnd(cmd);
	}

	void GaussianSplatScene::Clear()
	{
		splat_capacity = 0;
		model_capacity = 0;
		draw_capacity = 0;
		modelBuffer = {};
		instanceBuffer = {};
		indirectBuffer = {};
		sortedIndexBuffer = {};
		distanceBuffer = {};
		splatLookupBuffer = {};
	}
}
