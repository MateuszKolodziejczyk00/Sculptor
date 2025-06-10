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
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"


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


namespace render_sky_probe
{

DS_BEGIN(RenderSkyProbeDS, rg::RGDescriptorSetState<RenderSkyProbeDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_rwProbe)
DS_END();


static rdr::PipelineStateID CompileRenderSkyProbePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/RenderSkyProbe.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderSkyProbeCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderSkyProbePipeline"), shader);
}


static rg::RGTextureViewHandle RenderSkyProbe(rg::RenderGraphBuilder& graphBuilder, const AtmosphereContext& atmosphere, rg::RGTextureViewHandle skyViewLUT)
{
	SPT_PROFILER_FUNCTION();

	rg::TextureDef skyViewProbeDef(math::Vector2u(1u, 1u), rhi::EFragmentFormat::RGBA16_S_Float);
	skyViewProbeDef.type = rhi::ETextureType::Texture2D;
	const rg::RGTextureViewHandle skyProbe = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Sky Probe"), skyViewProbeDef);

	static const rdr::PipelineStateID pipeline = CompileRenderSkyProbePipeline();

	lib::MTHandle<RenderSkyProbeDS> ds = graphBuilder.CreateDescriptorSet<RenderSkyProbeDS>(RENDERER_RESOURCE_NAME("RenderSkyProbeDS"));
	ds->u_atmosphereParams = atmosphere.atmosphereParamsBuffer->CreateFullView();
	ds->u_skyViewLUT       = skyViewLUT;
	ds->u_rwProbe          = skyProbe;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Sky Probe"),
						  pipeline,
						  math::Vector2u(1u, 1u),
						  rg::BindDescriptorSets(std::move(ds)));
												

	return skyProbe;
}

} // render_sky_probe


namespace aerial_perspective
{

BEGIN_SHADER_STRUCT(RenderAerialPerspectiveConstants)
	SHADER_STRUCT_FIELD(Real32, participatingMediaNear)
	SHADER_STRUCT_FIELD(Real32, participatingMediaFar)
END_SHADER_STRUCT();


DS_BEGIN(RenderAerialPerspectiveDS, rg::RGDescriptorSetState<RenderAerialPerspectiveDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RenderAerialPerspectiveConstants>),       u_renderAPConstants)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),              u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<Real32>),                           u_dirLightShadowTerm)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector3f>),                   u_indirectInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),                            u_rwAerialPerspective)
DS_END();


static rdr::PipelineStateID CompileRenderAerialPerspectivePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/RenderAerialPerspective.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderAerialPerspectiveCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderAerialPerspectivePipeline"), shader);
}


static rg::RGTextureViewHandle RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const AtmosphereSceneSubsystem& atmosphereSubsystem, const RenderViewEntryDelegates::RenderAerialPerspectiveData& apData)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const AtmosphereContext& atmosphere = atmosphereSubsystem.GetAtmosphereContext();
	const AtmosphereParams& atmosphereParams = atmosphereSubsystem.GetAtmosphereParams();

	const math::Vector3u apRes = atmosphereParams.aerialPerspectiveParams.resolution;

	const rg::RGTextureViewHandle aerialPerspective = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Aerial Perspective"), rg::TextureDef(apRes, rhi::EFragmentFormat::RGBA16_S_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderAerialPerspectivePipeline();

	RenderAerialPerspectiveConstants shaderConstants;
	shaderConstants.participatingMediaNear = apData.fogParams->nearPlane;
	shaderConstants.participatingMediaFar  = apData.fogParams->farPlane;

	lib::MTHandle<RenderAerialPerspectiveDS> ds = graphBuilder.CreateDescriptorSet<RenderAerialPerspectiveDS>(RENDERER_RESOURCE_NAME("RenderAerialPerspectiveDS"));
	ds->u_atmosphereParams            = atmosphere.atmosphereParamsBuffer->CreateFullView();
	ds->u_renderAPConstants           = shaderConstants;
	ds->u_directionalLights           = atmosphere.directionalLightsBuffer->CreateFullView();
	ds->u_transmittanceLUT            = atmosphere.transmittanceLUT;
	ds->u_dirLightShadowTerm          = apData.fogParams->directionalLightShadowTerm;
	ds->u_indirectInScatteringTexture = apData.fogParams->indirectInScatteringTextureView;
	ds->u_rwAerialPerspective         = aerialPerspective;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Aerial Perspective"),
						  pipeline,
						  math::Vector2u(apRes.x(), apRes.y()),
						  rg::BindDescriptorSets(std::move(ds), renderView.GetRenderViewDS()));

	return aerialPerspective;
}

} // aerial_perspective


AtmosphereRenderSystem::AtmosphereRenderSystem()
{
	m_supportedStages = rsc::ERenderStage::DeferredShading;
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

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

	m_volumetricCloudsRenderer.RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);
}

void AtmosphereRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

	const rg::RGTextureViewHandle transmittanceLUT		= graphBuilder.AcquireExternalTextureView(context.transmittanceLUT);
	const rg::RGTextureViewHandle multiScatteringLUT	= graphBuilder.AcquireExternalTextureView(context.multiScatteringLUT);

	sky_view::SkyViewParams skyViewParams;
	skyViewParams.transmittanceLUT		= transmittanceLUT;
	skyViewParams.multiScatteringLUT	= multiScatteringLUT;
	skyViewParams.skyViewLUTResolution	= math::Vector2u(200u, 200u);

	const rg::RGTextureViewHandle skyViewLUT = sky_view::RenderSkyViewLUT(graphBuilder, viewSpec, context, skyViewParams);

	const rg::RGTextureViewHandle skyProbe = render_sky_probe::RenderSkyProbe(graphBuilder, context, skyViewLUT);
	
	shadingViewContext.skyViewLUT = skyViewLUT;
	shadingViewContext.skyProbe   = skyProbe;

	SPT_CHECK(viewSpec.SupportsStage(ERenderStage::PreRendering));

	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::RenderAerialPerspective).AddRawMember(this, &AtmosphereRenderSystem::RenderAerialPerspective);
}

void AtmosphereRenderSystem::RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const
{
	SPT_PROFILER_FUNCTION();

	const RenderViewEntryDelegates::RenderAerialPerspectiveData& apData = context.Get<RenderViewEntryDelegates::RenderAerialPerspectiveData>();

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	shadingViewContext.aerialPerspective = aerial_perspective::RenderAerialPerspective(graphBuilder, viewSpec, atmosphereSubsystem, apData);
}

} // spt::rsc
