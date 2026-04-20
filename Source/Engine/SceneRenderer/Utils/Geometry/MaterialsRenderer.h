#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


namespace materials_renderer
{

class MaterialBatchDS;

struct MaterialRenderCommand
{
	rdr::PipelineStateID           pipelineState;
	lib::MTHandle<MaterialBatchDS> materialBatchDS;
};


struct MaterialRenderCommands
{
	Uint16 geometryMaterialBatchesOffset = idxNone<Uint16>;
	Uint16 terrainMaterialBatchIdx       = idxNone<Uint16>;

	lib::DynamicArray<MaterialRenderCommand> commands;
};


struct MaterialsPassDefinition
{
	explicit MaterialsPassDefinition(const ViewRenderingSpec& inViewSpec)
		: viewSpec(inViewSpec)
	{
	}

	const ViewRenderingSpec& viewSpec;

	rg::RGBufferViewHandle visibleMeshlets;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle visibilityTexture;

	Bool                    enablePOM = true;
	rg::RGTextureViewHandle pomDepth;
};


void AppendGeometryMaterialsRenderCommands(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passParams, const GeometryPassDataCollection& geometryPassData, MaterialRenderCommands& renderCommands);
void AppendTerrainMaterialsRenderCommand(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passParams, const MaterialBatchPermutation& materialPermutation, MaterialRenderCommands& renderCommands);

void RenderMaterials(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passDef, const MaterialRenderCommands& renderCommands);

} // materials_renderer

} // spt::rsc
