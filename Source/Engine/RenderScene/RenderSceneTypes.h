#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "Bindless/BindlessTypes.h"
#include "StaticMeshes/StaticMeshGeometry.h"


namespace spt::rsc
{

class RenderScene;


struct TransformComponent
{
	TransformComponent()
		: m_transform(math::Affine3f::Identity())
		, m_uniformScale(1.f)
	{
	}

	TransformComponent(const math::Affine3f& transform)
	{
		SetTransform(transform);
	}

	void SetTransform(const math::Affine3f& newTransform)
	{
		m_transform = newTransform;

		const math::Matrix4f& transformMatrix = m_transform.matrix();

		const Real32 scaleX2 = transformMatrix.row(0).head<3>().squaredNorm();
		const Real32 scaleY2 = transformMatrix.row(1).head<3>().squaredNorm();
		const Real32 scaleZ2 = transformMatrix.row(2).head<3>().squaredNorm();

		const Real32 maxScale = std::max(std::max(scaleX2, scaleY2), scaleZ2);
		m_uniformScale = std::sqrt(maxScale);
	}

	const math::Affine3f& GetTransform() const
	{
		return m_transform;
	}

	Real32 GetUniformScale() const
	{
		return m_uniformScale;
	}

private:

	math::Affine3f m_transform;
	Real32 m_uniformScale;
};
SPT_REGISTER_COMPONENT_TYPE(TransformComponent, RenderSceneRegistry);


struct RenderInstanceData
{
	TransformComponent transformComp;
};


// we don't need 64 bytes for now, but we for sure will need more than 32, so 64 will be fine to properly align suballocations
BEGIN_SHADER_STRUCT(RenderEntityGPUData)
	SHADER_STRUCT_FIELD(math::Matrix4f, transform)
	SHADER_STRUCT_FIELD(Real32, uniformScale)
	SHADER_STRUCT_FIELD(math::Vector3f, padding0)
	SHADER_STRUCT_FIELD(math::Vector4f, padding1)
	SHADER_STRUCT_FIELD(math::Vector4f, padding2)
	SHADER_STRUCT_FIELD(math::Vector4f, padding3)
END_SHADER_STRUCT();


CREATE_NAMED_BUFFER(RenderEntitiesArray);
using RenderEntityGPUPtr = gfx::GPUNamedElemPtr<RenderEntitiesArray, RenderEntityGPUData>;


BEGIN_SHADER_STRUCT(GPUSceneFrameData)
	SHADER_STRUCT_FIELD(Real32,                    time)
	SHADER_STRUCT_FIELD(Real32,                    deltaTime)
	SHADER_STRUCT_FIELD(Uint32,                    frameIdx)
	SHADER_STRUCT_FIELD(RenderEntitiesArray,       renderEntitiesArray)
	SHADER_STRUCT_FIELD(StaticMeshGeometryBuffers, staticMeshGeometryBuffers)
END_SHADER_STRUCT();


DS_BEGIN(RenderSceneDS, rg::RGDescriptorSetState<RenderSceneDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RenderEntityGPUData>),         u_renderEntitiesData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<GPUSceneFrameData>), u_gpuSceneFrameConstants)
DS_END();

} // spt::rsc