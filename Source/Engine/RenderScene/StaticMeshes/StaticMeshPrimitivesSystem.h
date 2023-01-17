#pragma once

#include "PrimitivesSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshGPUInstanceRenderData)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
	SHADER_STRUCT_FIELD(Uint32, staticMeshIdx)
END_SHADER_STRUCT();


struct StaticMeshInstanceRenderData
{
	Uint32 staticMeshIdx;
	Uint32 trianglesNum;
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
