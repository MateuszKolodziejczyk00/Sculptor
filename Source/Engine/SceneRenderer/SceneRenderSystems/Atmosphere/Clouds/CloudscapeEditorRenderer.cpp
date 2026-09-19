#include "CloudscapeEditorRenderer.h"


namespace spt::rsc::editor
{

BEGIN_SHADER_STRUCT(CloudscapeInfluenceGizmoConstants)
	SHADER_STRUCT_FIELD(math::Vector2f,                    mouseUV)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(Real32,                            radius)
	SHADER_STRUCT_FIELD(math::Vector3f,                    color)
	SHADER_STRUCT_FIELD(Real32,                            opacity)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(CloudscapeInfluenceGizmoPSO, "Sculptor/Atmosphere/VolumetricClouds/CloudscapeInfluenceGizmo.hlsl", CloudscapeInfluenceGizmoCS)


void RenderCloudscapeInfluenceGizmo(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const CloudscapePaintingData& paintingData, const RenderViewEntryDelegates::DebugRenderAndEditorData& params, const EditorRendering& edParams)
{
	SPT_PROFILER_FUNCTION();

	CloudscapeInfluenceGizmoConstants shaderConstants;
	shaderConstants.mouseUV    = edParams.mouseUV;
	shaderConstants.resolution = params.color->GetResolution2D();
	shaderConstants.radius     = edParams.cloudscape.influenceGizmo->radius;
	shaderConstants.color      = edParams.cloudscape.influenceGizmo->color;
	shaderConstants.opacity    = edParams.cloudscape.influenceGizmo->opacity;
	shaderConstants.depth      = params.depth;
	shaderConstants.rwOutput   = params.color;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Cloudscape Influence Gizmo"),
						  CloudscapeInfluenceGizmoPSO::pso,
						  math::Utils::DivideCeil(params.color->GetResolution2D(), math::Vector2u(16u, 16u)),
						  shaderConstants);
}


BEGIN_SHADER_STRUCT(WeatherMapPaintCommandConstants)
	SHADER_STRUCT_FIELD(math::Vector2f,                    mouseUV)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(Real32,                            radius)
	SHADER_STRUCT_FIELD(Real32,                            paintValue)
	SHADER_STRUCT_FIELD(Uint32,                            channelToPaint)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         weatherMapNoise)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwWeatherMap)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(TerrainMaterialPaintCommandPSO, "Sculptor/Atmosphere/VolumetricClouds/WeatherMapPaintCommand.hlsl", WeatherMapPaintCommandCS)


void ExecuteWeatherMapPaintCommand(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const CloudscapePaintingData& paintingData, const EditorRendering& edParams)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& viewContext = view.GetShadingViewContext();

	const rg::RGTextureViewHandle weatherMap = edParams.cloudscape.paintedWeatherMap;
	SPT_CHECK(weatherMap.IsValid());

	const math::Vector2u resolution = weatherMap->GetResolution2D();

	WeatherMapPaintCommandConstants shaderConstants;
	shaderConstants.mouseUV         = edParams.mouseUV;
	shaderConstants.resolution      = resolution;
	shaderConstants.radius          = edParams.cloudscape.influenceGizmo->radius;
	shaderConstants.paintValue      = edParams.cloudscape.weatherMapPaintCommand->value;
	shaderConstants.channelToPaint  = edParams.cloudscape.weatherMapPaintCommand->channelToPaint;
	shaderConstants.weatherMapNoise = paintingData.weatherMapNoise;
	shaderConstants.depth           = viewContext.depth;
	shaderConstants.rwWeatherMap    = edParams.cloudscape.paintedWeatherMap;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Execute Weather Map Paint Command"),
						  TerrainMaterialPaintCommandPSO::pso,
						  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
						  shaderConstants);
}

} // spt::rsc::editor
