#pragma once

#include "Containers/PagedGenerationalPool.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "Bindless/BindlessTypes.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "RenderSceneRegistry.h"
#include "GeometryManager.h"


namespace spt::rsc
{

class RenderScene;


struct RenderInstance
{
	math::Affine3f transform;
};


using RenderInstanceHandle = lib::PagedGenerationalPool<RenderInstance>::Handle;


// we don't need 64 bytes for now, but we for sure will need more than 32, so 64 will be fine to properly align suballocations
BEGIN_SHADER_STRUCT(RenderEntityGPUData)
	SHADER_STRUCT_FIELD(math::Matrix4f, transform)
	SHADER_STRUCT_FIELD(Real32, uniformScale)
	SHADER_STRUCT_FIELD(math::Vector3f, padding0)
	SHADER_STRUCT_FIELD(math::Vector4f, padding1)
	SHADER_STRUCT_FIELD(math::Vector4f, padding2)
	SHADER_STRUCT_FIELD(math::Vector4f, padding3)
END_SHADER_STRUCT();


CREATE_NAMED_BUFFER(RenderEntitiesArray, RenderEntityGPUData);
using RenderEntityGPUPtr = gfx::GPUNamedElemPtr<RenderEntitiesArray, RenderEntityGPUData>;


inline RenderEntityGPUPtr GetInstanceGPUDataPtr(RenderInstanceHandle instance)
{
	return RenderEntityGPUPtr(instance.idx * sizeof(RenderEntityGPUData));
}

} // spt::rsc
