#pragma once

#include "SculptorCoreTypes.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "RayTracing/RayTracingGeometry.h"
#include "RenderSceneMacros.h"
#include "RenderSceneTypes.h"
#include "Containers/PagedGenerationalPool.h"


namespace spt::rsc
{


struct RTInstance
{
	RenderInstanceHandle              instance;
	const RayTracingGeometryProvider& rtGeometry;
	MaterialSlotsChunkHandle          materialSlots;
};


using RTInstanceHandle = lib::PagedGenerationalPool<RTInstance>::Handle;


struct RTScene
{
	RTScene()
		: instances("RTScene_InstancesPool")
	{
	}

	lib::PagedGenerationalPool<RTInstance> instances;
};

} // spt::rsc
