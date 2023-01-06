#pragma once

#include "RenderSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "SculptorECS.h"

namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(, 16, StaticMeshGPURenderData)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
	SHADER_STRUCT_FIELD(Uint32, firstPrimitiveIdx)
	SHADER_STRUCT_FIELD(Uint32, primitivesNum)
END_SHADER_STRUCT();


/**
 * CPU side render data
 */
struct StaticMeshRenderData
{
	Uint32 firstPrimitiveIdx;
	Uint32 primitivesNum;
};


/**
 * Handle to allocation of GPU render data
 */
struct StaticMeshRenderDataHandle
{
	rhi::RHISuballocation basePassInstanceData;
};


using StaticMeshInstancesList = RenderInstancesList<StaticMeshGPURenderData>;


class StaticMeshRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshRenderSystem();

protected:

	// Begin RenderSystem overrides
	virtual void OnInitialize(RenderScene& renderScene) override;
	// End RenderSystem overrides

private:

	void PostBasePassSMConstructed(ecs::Registry& registry, ecs::Entity entity);
	void PreBasePassSMDestroyed(ecs::Registry& registry, ecs::Entity entity);

	StaticMeshInstancesList m_basePassInstances;
};

} // spt::rsc
