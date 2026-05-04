#pragma once

#include "RenderSceneTypes.h"
#include "SculptorCoreTypes.h"
#include "Containers/PagedGenerationalPool.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "StaticMeshes/RenderMesh.h"


namespace spt::rsc
{

struct RetainedDraw
{
	RenderInstanceHandle     instance;
	const RenderMesh&        mesh;
	MaterialSlotsChunkHandle materialSlots;
};


using RetainedDrawHandle = lib::PagedGenerationalPool<RetainedDraw>::Handle;


struct RetainedDraws
{
	RetainedDraws()
		: draws("RetainedDraws_DrawsPool")
	{ }

	lib::PagedGenerationalPool<RetainedDraw> draws;
};

} // spt::rsc
