#pragma once

#include "PrimitivesSystem.h"
#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"

namespace spt::rsc
{

class RenderView;


struct StaticMeshInstanceRenderData
{
	Uint32 staticMeshIdx;
};


BEGIN_SHADER_STRUCT(, StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(Uint32, instanceIdx)
	SHADER_STRUCT_FIELD(Uint32, staticMeshIdx)
END_SHADER_STRUCT();


struct StaticMeshBatch
{
	StaticMeshBatch()
		: maxSubmeshesNum(0)
		, maxMeshletsNum(0)
		, maxTrianglesNum(0)
	{ }

	lib::DynamicArray<StaticMeshBatchElement> batchElements;
	Uint32 maxSubmeshesNum;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;
};


class RENDER_SCENE_API StaticMeshPrimitivesSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit StaticMeshPrimitivesSystem(RenderScene& owningScene);

	StaticMeshBatch BuildBatchForView(const RenderView& view) const;
};

} // spt::rsc
