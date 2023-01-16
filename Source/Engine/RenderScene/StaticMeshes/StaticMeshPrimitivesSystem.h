#pragma once

#include "PrimitivesSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"

namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(, 16, StaticMeshGPUInstanceRenderData)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
	SHADER_STRUCT_FIELD(Uint32, staticMeshDataOffset)
END_SHADER_STRUCT();


BEGIN_ALIGNED_SHADER_STRUCT(, 16, StaticMeshGPURenderData)
	SHADER_STRUCT_FIELD(Uint32, submeshesNum)
	SHADER_STRUCT_FIELD(Uint32, submeshesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletsOffset)
	SHADER_STRUCT_FIELD(Uint32, geometryDataOffset)
END_SHADER_STRUCT();


struct StaticMeshInstanceRenderData
{
	Uint32 staticMeshDataOffset;
};


/**
 * Handle to allocation of GPU render data
 */
struct StaticMeshRenderDataHandle
{
	rhi::RHISuballocation staticMeshGPUInstanceData;
};


using StaticMeshInstancesList = RenderInstancesList<StaticMeshGPUInstanceRenderData>;


class RENDER_SCENE_API StaticMeshPrimitivesSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit StaticMeshPrimitivesSystem(RenderScene& owningScene);
	virtual ~StaticMeshPrimitivesSystem();

	virtual void Update() override;
	
	const StaticMeshInstancesList& GetStaticMeshInstances() const;

private:

	void OnStaticMeshUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnStaticMeshDestryed(RenderSceneRegistry& registry, RenderSceneEntity entity);

	StaticMeshInstancesList m_staticMeshInstances;
};

} // spt::rsc
