#pragma once

#include "PrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"
#include "RenderingDataRegistry.h"

namespace spt::rsc
{

class RenderView;


struct StaticMeshInstanceRenderData
{
	RenderingDataEntityHandle staticMesh;
};


BEGIN_SHADER_STRUCT(, StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
END_SHADER_STRUCT();


struct StaticMeshBatchDefinition
{
	StaticMeshBatchDefinition()
		: maxMeshletsNum(0)
		, maxTrianglesNum(0)
	{ }

	Bool IsValid() const
	{
		return !batchElements.empty();
	}

	lib::DynamicArray<StaticMeshBatchElement> batchElements;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;
};


class RENDER_SCENE_API StaticMeshPrimitivesSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit StaticMeshPrimitivesSystem(RenderScene& owningScene);

	StaticMeshBatchDefinition BuildBatchForView(const RenderView& view) const;
};

} // spt::rsc
