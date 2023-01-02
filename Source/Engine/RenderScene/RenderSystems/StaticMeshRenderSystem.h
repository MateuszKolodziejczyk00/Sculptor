#pragma once

#include "RenderSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "SculptorECS.h"

namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(, 16, BasePassStaticMeshRenderData)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
	SHADER_STRUCT_FIELD(Uint32, firstPrimitiveIdx)
	SHADER_STRUCT_FIELD(Uint32, primitivesNum)
END_SHADER_STRUCT();


struct BasePassStaticMeshRenderDataHandle
{
	rhi::RHISuballocation basePassInstanceData;
};


using BasePassStaticMeshInstancesList = RenderInstancesList<BasePassStaticMeshRenderData>;


class StaticMeshRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshRenderSystem();

	// Begin RenderSystem overrides
	virtual void Initialize(RenderScene& renderScene) override;
	virtual void Update(const RenderScene& renderScene, float dt) override;
	// End RenderSystem overrides

private:

	void PostBasePassSMConstructed(ecs::Registry& registry, ecs::Entity entity);
	void PreBasePassSMDestroyed(ecs::Registry& registry, ecs::Entity entity);

	BasePassStaticMeshInstancesList m_basePassInstances;
};

} // spt::rsc
