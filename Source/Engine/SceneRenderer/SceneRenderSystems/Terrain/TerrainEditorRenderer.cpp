#include "TerrainEditorRenderer.h"


namespace spt::rsc::editor
{

BEGIN_SHADER_STRUCT(TerrainInfluenceGizmoConstants)
	SHADER_STRUCT_FIELD(math::Vector2f,                    mouseUV)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(Real32,                            radius)
	SHADER_STRUCT_FIELD(math::Vector3f,                    color)
	SHADER_STRUCT_FIELD(Real32,                            opacity)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(TerrainInfluenceGizmoPSO, "Sculptor/Terrain/TerrainInfluenceGizmo.hlsl", TerrainInfluenceGizmoCS)


void RenderInfluenceGizmo(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const RenderViewEntryDelegates::DebugRenderAndEditorData& params, const EditorRendering& edParams)
{
	SPT_PROFILER_FUNCTION();

	TerrainInfluenceGizmoConstants shaderConstants;
	shaderConstants.mouseUV    = edParams.mouseUV;
	shaderConstants.resolution = params.color->GetResolution2D();
	shaderConstants.radius     = edParams.terrain.influenceGizmo->radius;
	shaderConstants.color      = edParams.terrain.influenceGizmo->color;
	shaderConstants.opacity    = edParams.terrain.influenceGizmo->opacity;
	shaderConstants.depth      = params.depth;
	shaderConstants.rwOutput   = params.color;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Terrain Influence Gizmo"),
						  TerrainInfluenceGizmoPSO::pso,
						  math::Utils::DivideCeil(params.color->GetResolution2D(), math::Vector2u(16u, 16u)),
						  rg::EmptyDescriptorSets(),
						  shaderConstants);
}


BEGIN_SHADER_STRUCT(TerrainMaterialPaintCommandConstants)
	SHADER_STRUCT_FIELD(math::Vector2f,            mouseUV)
	SHADER_STRUCT_FIELD(math::Vector2u,            resolution)
	SHADER_STRUCT_FIELD(Real32,                    radius)
	SHADER_STRUCT_FIELD(Uint32,                    materialID)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>, rwMaterialIDs)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(TerrainMaterialPaintCommandPSO, "Sculptor/Terrain/TerrainMaterialPaintCommand.hlsl", TerrainMaterialPaintCommandCS)


void ExecuteMaterialPaintCommand(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const EditorRendering& edParams)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& viewContext = view.GetShadingViewContext();

	const rg::RGTextureViewHandle materialIDs = edParams.terrain.paintedMaterialMap;
	SPT_CHECK(materialIDs.IsValid());

	const math::Vector2u resolution = materialIDs->GetResolution2D();

	TerrainMaterialPaintCommandConstants shaderConstants;
	shaderConstants.mouseUV       = edParams.mouseUV;
	shaderConstants.resolution    = resolution;
	shaderConstants.radius        = edParams.terrain.influenceGizmo->radius;
	shaderConstants.materialID    = edParams.terrain.materialPaintCommand->materialIDToPaint;
	shaderConstants.depth         = viewContext.depth;
	shaderConstants.rwMaterialIDs = edParams.terrain.paintedMaterialMap;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Execute Terrain Material Paint Command"),
						  TerrainMaterialPaintCommandPSO::pso,
						  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
						  rg::EmptyDescriptorSets(),
						  shaderConstants);
}

} // spt::rsc::editor
