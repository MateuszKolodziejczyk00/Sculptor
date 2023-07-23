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


static void RenderTransmittanceLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittanceLUT)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileRenderTransmittanceLUTPipeline();
	
	lib::SharedPtr<RenderTransmittanceLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderTransmittanceLUTDS>(RENDERER_RESOURCE_NAME("Render Transmittance LUT DS"));
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

	lib::SharedPtr<RenderMultiScatteringLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderMultiScatteringLUTDS>(RENDERER_RESOURCE_NAME("Render Multi Scattering LUT DS"));
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

	const rhi::TextureDefinition lutDefinition(skyViewLUTResolution, lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture), rhi::EFragmentFormat::B10G11R11_U_Float);
	const rg::RGTextureViewHandle skyViewLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Sky View LUT"), lutDefinition, rhi::EMemoryUsage::GPUOnly);

	static const rdr::PipelineStateID pipeline = CompileRenderSkyViewLUTPipeline();

	lib::SharedPtr<RenderSkyViewLUTDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<RenderSkyViewLUTDS>(RENDERER_RESOURCE_NAME("Render Sky View LUT DS"));
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

namespace apply_atmosphere
{

DS_BEGIN(ApplyAtmosphereDS, rg::RGDescriptorSetState<ApplyAtmosphereDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),					u_directionalLights)
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


static void ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& view, const AtmosphereContext& context, rg::RGTextureViewHandle skyViewLUT)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = view.GetRenderView();

	const math::Vector2u renderingResolution = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData = view.GetData().Get<DepthPrepassData>();
	const ShadingData& shadingData = view.GetData().Get<ShadingData>();

	static rdr::PipelineStateID pipeline = CompileApplyAtmospherePipeline();

	lib::SharedPtr<ApplyAtmosphereDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<ApplyAtmosphereDS>(RENDERER_RESOURCE_NAME("Apply Atmosphere DS"));
	ds->u_atmosphereParams	= context.atmosphereParamsBuffer->CreateFullView();
	ds->u_directionalLights	= context.directionalLightsBuffer->CreateFullView();
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

	const auto createLUT = [](const rdr::RendererResourceName& name, math::Vector2u resolution, rhi::EFragmentFormat format)
	{
		const rhi::TextureDefinition textureDef(resolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), format);
		const lib::SharedRef<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(name, textureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		return texture->CreateView(RENDERER_RESOURCE_NAME("Sky Illuminance Texture View"), viewDefinition);
	};

	m_atmosphereTransmittanceLUT	= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Transmittance LUT"), math::Vector2u(256u, 64u), rhi::EFragmentFormat::B10G11R11_U_Float);
	m_multiScatteringLUT			= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Multi Scattering LUT"), math::Vector2u(32u, 32u), rhi::EFragmentFormat::B10G11R11_U_Float);
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene);

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	if (atmosphereSubsystem.IsAtmosphereTextureDirty())
	{
		const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

		const rg::RGTextureViewHandle transmittanceLUT		= graphBuilder.AcquireExternalTextureView(m_atmosphereTransmittanceLUT);
		const rg::RGTextureViewHandle multiScatteringLUT	= graphBuilder.AcquireExternalTextureView(m_multiScatteringLUT);

		transmittance_lut::RenderTransmittanceLUT(graphBuilder, renderScene, context, transmittanceLUT);

		multi_scattering_lut::RenderMultiScatteringLUT(graphBuilder, renderScene, context, transmittanceLUT, multiScatteringLUT);
	}
}

void AtmosphereRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& context = atmosphereSubsystem.GetAtmosphereContext();

	sky_view::SkyViewParams skyViewParams;
	skyViewParams.transmittanceLUT		= graphBuilder.AcquireExternalTextureView(m_atmosphereTransmittanceLUT);
	skyViewParams.multiScatteringLUT	= graphBuilder.AcquireExternalTextureView(m_multiScatteringLUT);
	skyViewParams.skyViewLUTResolution	= math::Vector2u(200u, 200u);

	const rg::RGTextureViewHandle skyViewLUT = sky_view::RenderSkyViewLUT(graphBuilder, viewSpec, context, skyViewParams);

	ViewAtmosphereRenderData viewAtmosphere;
	viewAtmosphere.skyViewLUT = skyViewLUT;

	viewSpec.GetData().Create<ViewAtmosphereRenderData>(viewAtmosphere);

	RenderStageEntries& atmosphereStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ApplyAtmosphere);
	atmosphereStageEntries.GetOnRenderStage().AddRawMember(this, &AtmosphereRenderSystem::ApplyAtmosphereToView);
}

void AtmosphereRenderSystem::ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const ViewAtmosphereRenderData& viewAtmosphere = viewSpec.GetData().Get<ViewAtmosphereRenderData>();

	AtmosphereSceneSubsystem& atmosphereSubsystem = scene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	apply_atmosphere::ApplyAtmosphereToView(graphBuilder, viewSpec, atmosphereSubsystem.GetAtmosphereContext(), viewAtmosphere.skyViewLUT);
}

} // spt::rsc
