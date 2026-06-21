#pragma once

#include "SculptorCoreTypes.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "RayTracing/RayTracingGeometry.h"
#include "RenderSceneMacros.h"
#include "RenderSceneTypes.h"
#include "Containers/PagedGenerationalPool.h"


namespace spt::rsc
{

enum class ERTInstanceType : Uint16
{
	StaticMesh,
	Terrain
};


struct RTInstance
{
	RenderInstanceHandle              instance;
	ERTInstanceType                   type = ERTInstanceType::StaticMesh;
	const RayTracingGeometryProvider& rtGeometry;
	MaterialSlotsChunkHandle          materialSlots;
};


using RTInstanceHandle = lib::PagedGenerationalPool<RTInstance>::Handle;


struct RTScene
{
	static constexpr Uint32 committedInstancesNum = 1024u;
	static constexpr Uint32 maxInstancesNum       = 32u * 1024u;

	RTScene()
		: instances("RTScene_InstancesPool", committedInstancesNum, maxInstancesNum)
	{
	}

	lib::PagedGenerationalPool<RTInstance> instances;
};

} // spt::rsc
