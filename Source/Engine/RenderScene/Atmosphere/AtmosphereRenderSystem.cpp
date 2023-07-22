#include "AtmosphereRenderSystem.h"
#include "AtmosphereSceneSubsystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "Lights/LightTypes.h"
#include "SceneRenderer/SceneRendererTypes.h"


namespace spt::rsc
{

namespace transmittance_lut
{

DS_BEGIN(RenderTransmittanceLUTDS, rg::RGDescriptorSetState<RenderTransmittanceLUTDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),			u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),	u_atmosphereParams)
DS_END();


static rdr::PipelineStateID CompileRenderTransmittanceLUTPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/TransmittanceLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderTransmittanceLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderTransmittanceLUTPipeline"), shader);
}


static rg::RGTextureViewHandle RenderTransmittanceLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, math::Vector2u lutResolution)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition transmittanceLUTDef(lutResolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), rhi::EFragmentFormat::B10G11R11_U_Float);
	const rg::RGTextureViewHandle transmittanceLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Atmosphere Transmittance LUT"), transmittanceLUTDef, rhi::EMemoryUsage::GPUOnly);

	static const rdr::PipelineStateID pipeline = CompileRenderTransmittanceLUTPipeline();
	
	lib::SharedPtr<RenderTransmittanceLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderTransmittanceLUTDS>(RENDERER_RESOURCE_NAME("Render Transmittance LUT DS"));
	ds->u_transmittanceLUT = transmittanceLUT;
	ds->u_atmosphereParams = context.atmosphereParamsBuffer->CreateFullView();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Transmittance LUT"),
						  pipeline,
						  math::Utils::DivideCeil(lutResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));

	return transmittanceLUT;
}

} // transmittance_lut

namespace multi_scattering_lut
{

DS_BEGIN(RenderMultiScatteringLUTDS, rg::RGDescriptorSetState<RenderMultiScatteringLUTDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_multiScatteringLUT)
DS_END();


static rdr::PipelineStateID CompileRenderMultiScatteringLUTPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/MultiScatteringLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderMultiScatteringLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderMultiScatteringLUTPipeline"), shader);
}


static rg::RGTextureViewHandle RenderMultiScatteringLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittancleLUT, math::Vector2u lutResolution)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition multiScatteringLUTDef(lutResolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), rhi::EFragmentFormat::B10G11R11_U_Float);
	const rg::RGTextureViewHandle multiScatteringLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Atmosphere Multi Scattering LUT"), multiScatteringLUTDef, rhi::EMemoryUsage::GPUOnly);

	static const rdr::PipelineStateID pipeline = CompileRenderMultiScatteringLUTPipeline();

	lib::SharedPtr<RenderMultiScatteringLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderMultiScatteringLUTDS>(RENDERER_RESOURCE_NAME("Render Multi Scattering LUT DS"));
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_transmittanceLUT		= transmittancleLUT;
	ds->u_multiScatteringLUT	= multiScatteringLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Multi Scattering LUT"),
						  pipeline,
						  math::Utils::DivideCeil(lutResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));

	return multiScatteringLUT;
}

} // multi_scattering_lut

namespace sky_view
{

struct SkyViewParams
{
	rg::RGTextureViewHandle transmittanceLUT;
	rg::RGTextureViewHandle multiScatteringLUT;
	lib::SharedPtr<rdr::TextureView> skyViewLUT;
};


DS_BEGIN(RenderSkyViewLUTDS, rg::RGDescriptorSetState<RenderSkyViewLUTDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),					u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_multiScatteringLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_skyViewLUT)
DS_END();


static rdr::PipelineStateID CompileRenderSkyViewLUTPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/SkyViewLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderSkyViewLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderSkyViewLUTPipeline"), shader);
}


static void RenderSkyViewLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, const SkyViewParams& skyViewParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!skyViewParams.skyViewLUT);

	const math::Vector2u skyViewResolution = skyViewParams.skyViewLUT->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileRenderSkyViewLUTPipeline();

	lib::SharedPtr<RenderSkyViewLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderSkyViewLUTDS>(RENDERER_RESOURCE_NAME("Render Sky View LUT DS"));
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_directionalLights		= context.directionalLightsBuffer->CreateFullView();
	ds->u_transmittanceLUT		= skyViewParams.transmittanceLUT;
	ds->u_multiScatteringLUT	= skyViewParams.multiScatteringLUT;
	ds->u_skyViewLUT			= skyViewParams.skyViewLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Sky View LUT"),
						  pipeline,
						  math::Utils::DivideCeil(skyViewResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // sky_view

namespace apply_atmosphere
{

DS_BEGIN(ApplyAtmosphereDS, rg::RGDescriptorSetState<ApplyAtmosphereDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_luminanceTexture)
DS_END();


static rdr::PipelineStateID CompileApplyAtmospherePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/ApplyAtmosphere.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplyAtmosphereCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ApplyAtmospherePipeline"), shader);
}


static void ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& view, const AtmosphereContext& context, const lib::SharedPtr<rdr::TextureView>& skyViewLUT)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = view.GetRenderView();

	const math::Vector2u renderingResolution = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData = view.GetData().Get<DepthPrepassData>();
	const ShadingData& shadingData = view.GetData().Get<ShadingData>();

	static rdr::PipelineStateID pipeline = CompileApplyAtmospherePipeline();

	lib::SharedPtr<ApplyAtmosphereDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<ApplyAtmosphereDS>(RENDERER_RESOURCE_NAME("Apply Atmosphere DS"));
	ds->u_atmosphereParams	= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_skyViewLUT		= skyViewLUT;
	ds->u_depthTexture		= depthPrepassData.depth;
	ds->u_luminanceTexture	= shadingData.luminanceTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Atmosphere"),
						  pipeline,
						  math::Utils::DivideCeil(renderingResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 renderView.GetRenderViewDS()));
}

} // apply_atmosphere


AtmosphereRenderSystem::AtmosphereRenderSystem()
{
	m_supportedStages = rsc::ERenderStage::ApplyAtmosphere;

	const math::Vector3u skyIlluminanceTextureSize(200, 100, 1);
	const rhi::TextureDefinition skyIlluminanceTextureDef(skyIlluminanceTextureSize, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), rhi::EFragmentFormat::B10G11R11_U_Float);
	const lib::SharedRef<rdr::Texture> skyIlluminanceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Sky Illuminance Texture"), skyIlluminanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_skyIlluminanceTextureView = skyIlluminanceTexture->CreateView(RENDERER_RESOURCE_NAME("Sky Illuminance Texture View"), viewDefinition);
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene);

	AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	if (atmosphereSubsystem.IsAtmosphereTextureDirty())
	{
		const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

		const math::Vector2u transmittanceLUTRes(256u, 64u);
		const math::Vector2u multiScatteringLUTRes(32u, 32u);

		const rg::RGTextureViewHandle transmittanceLUT = transmittance_lut::RenderTransmittanceLUT(graphBuilder, renderScene, context, transmittanceLUTRes);

		const rg::RGTextureViewHandle multiScatteringLUT = multi_scattering_lut::RenderMultiScatteringLUT(graphBuilder, renderScene, context, transmittanceLUT, multiScatteringLUTRes);

		sky_view::SkyViewParams skyViewParams;
		skyViewParams.skyViewLUT			= m_skyIlluminanceTextureView;
		skyViewParams.transmittanceLUT		= transmittanceLUT;
		skyViewParams.multiScatteringLUT	= multiScatteringLUT;

		sky_view::RenderSkyViewLUT(graphBuilder, renderScene, context, skyViewParams);
	}
}

void AtmosphereRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	RenderStageEntries& atmosphereStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ApplyAtmosphere);
	atmosphereStageEntries.GetOnRenderStage().AddRawMember(this, &AtmosphereRenderSystem::ApplyAtmosphereToView);
}

void AtmosphereRenderSystem::ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	AtmosphereSceneSubsystem& atmosphereSubsystem = scene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	apply_atmosphere::ApplyAtmosphereToView(graphBuilder, viewSpec, atmosphereSubsystem.GetAtmosphereContext(), m_skyIlluminanceTextureView);
}

} // spt::rsc
