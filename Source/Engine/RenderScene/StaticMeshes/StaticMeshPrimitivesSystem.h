#pragma once

#include "PrimitivesSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"

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


class StaticMeshPrimitivesSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit StaticMeshPrimitivesSystem(RenderScene& owningScene);
	~StaticMeshPrimitivesSystem();

	virtual void Update() override;

private:

	void OnStaticMeshUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnStaticMeshDestryed(RenderSceneRegistry& registry, RenderSceneEntity entity);

	StaticMeshInstancesList m_staticMeshInstances;
};

} // spt::rsc
