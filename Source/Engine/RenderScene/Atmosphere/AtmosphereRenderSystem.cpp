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


static void RenderTransmittanceLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittanceLUT)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileRenderTransmittanceLUTPipeline();
	
	lib::MTHandle<RenderTransmittanceLUTDS> ds = graphBuilder.CreateDescriptorSet<RenderTransmittanceLUTDS>(RENDERER_RESOURCE_NAME("Render Transmittance LUT DS"));
	ds->u_transmittanceLUT = transmittanceLUT;
	ds->u_atmosphereParams = context.atmosphereParamsBuffer->CreateFullView();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Transmittance LUT"),
						  pipeline,
						  math::Utils::DivideCeil(transmittanceLUT->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
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


static void RenderMultiScatteringLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittancleLUT, rg::RGTextureViewHandle multiScatteringLUT)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileRenderMultiScatteringLUTPipeline();

	lib::MTHandle<RenderMultiScatteringLUTDS> ds = graphBuilder.CreateDescriptorSet<RenderMultiScatteringLUTDS>(RENDERER_RESOURCE_NAME("Render Multi Scattering LUT DS"));
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_transmittanceLUT		= transmittancleLUT;
	ds->u_multiScatteringLUT	= multiScatteringLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Multi Scattering LUT"),
						  pipeline,
						  math::Utils::DivideCeil(multiScatteringLUT->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // multi_scattering_lut

namespace sky_view
{

struct SkyViewParams
{
	rg::RGTextureViewHandle transmittanceLUT;
	rg::RGTextureViewHandle multiScatteringLUT;
	math::Vector2u skyViewLUTResolution;
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


static rg::RGTextureViewHandle RenderSkyViewLUT(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, const AtmosphereContext& context, const SkyViewParams& skyViewParams)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u skyViewLUTResolution = skyViewParams.skyViewLUTResolution;

	const rg::RGTextureViewHandle skyViewLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Sky View LUT"), rg::TextureDef(skyViewLUTResolution, rhi::EFragmentFormat::RGBA16_S_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderSkyViewLUTPipeline();

	lib::MTHandle<RenderSkyViewLUTDS> ds = graphBuilder.CreateDescriptorSet<RenderSkyViewLUTDS>(RENDERER_RESOURCE_NAME("Render Sky View LUT DS"));
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_directionalLights		= context.directionalLightsBuffer->CreateFullView();
	ds->u_transmittanceLUT		= skyViewParams.transmittanceLUT;
	ds->u_multiScatteringLUT	= skyViewParams.multiScatteringLUT;
	ds->u_skyViewLUT			= skyViewLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Sky View LUT"),
						  pipeline,
						  math::Utils::DivideCeil(skyViewLUTResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 viewSpec.GetRenderView().GetRenderViewDS()));

	return skyViewLUT;
}

} // sky_view


AtmosphereRenderSystem::AtmosphereRenderSystem()
{
	m_supportedStages = rsc::ERenderStage::DeferredShading;
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs);

	AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	const Bool shouldUpdateTransmittanceLUT = atmosphereSubsystem.ShouldUpdateTransmittanceLUT();
	const Bool shouldUpdateMultiScatteringLUT = atmosphereSubsystem.ShouldUpdateTransmittanceLUT() || atmosphereSubsystem.IsAtmosphereTextureDirty();

	const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

	const rg::RGTextureViewHandle transmittanceLUT		= graphBuilder.AcquireExternalTextureView(context.transmittanceLUT);
	const rg::RGTextureViewHandle multiScatteringLUT	= graphBuilder.AcquireExternalTextureView(context.multiScatteringLUT);

	if (shouldUpdateTransmittanceLUT)
	{
		transmittance_lut::RenderTransmittanceLUT(graphBuilder, renderScene, context, transmittanceLUT);
		atmosphereSubsystem.PostUpdateTransmittanceLUT();
	}

	if (shouldUpdateMultiScatteringLUT)
	{
		multi_scattering_lut::RenderMultiScatteringLUT(graphBuilder, renderScene, context, transmittanceLUT, multiScatteringLUT);
	}

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
}

void AtmosphereRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

	const rg::RGTextureViewHandle transmittanceLUT		= graphBuilder.AcquireExternalTextureView(context.transmittanceLUT);
	const rg::RGTextureViewHandle multiScatteringLUT	= graphBuilder.AcquireExternalTextureView(context.multiScatteringLUT);

	sky_view::SkyViewParams skyViewParams;
	skyViewParams.transmittanceLUT		= transmittanceLUT;
	skyViewParams.multiScatteringLUT	= multiScatteringLUT;
	skyViewParams.skyViewLUTResolution	= math::Vector2u(200u, 200u);

	const rg::RGTextureViewHandle skyViewLUT = sky_view::RenderSkyViewLUT(graphBuilder, viewSpec, context, skyViewParams);
	
	viewSpec.GetShadingViewContext().skyViewLUT = skyViewLUT;
}

} // spt::rsc
