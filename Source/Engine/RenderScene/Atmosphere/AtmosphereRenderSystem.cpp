#include "AtmosphereRenderSystem.h"
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
	ds->u_atmosphereParams = context.atmosphereParamsBuffer->GetFullView();

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
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->GetFullView();
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
	ds->u_atmosphereParams		= context.atmosphereParamsBuffer->GetFullView();
	ds->u_directionalLights		= context.directionalLightsBuffer->GetFullView();
	ds->u_transmittanceLUT		= skyViewParams.transmittanceLUT;
	ds->u_multiScatteringLUT	= skyViewParams.multiScatteringLUT;
	ds->u_skyViewLUT			= skyViewLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Sky View LUT"),
						  pipeline,
						  math::Utils::DivideCeil(skyViewLUTResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));

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
	ds->u_atmosphereParams = atmosphere.atmosphereParamsBuffer->GetFullView();
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


static rg::RGTextureViewHandle RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const AtmosphereRenderSystem& atmosphereSystem, const RenderViewEntryDelegates::RenderAerialPerspectiveData& apData)
{
	SPT_PROFILER_FUNCTION();

	const AtmosphereContext& atmosphere      = atmosphereSystem.GetAtmosphereContext();
	const AtmosphereParams& atmosphereParams = atmosphereSystem.GetAtmosphereParams();

	const math::Vector3u apRes = atmosphereParams.aerialPerspectiveParams.resolution;

	const rg::RGTextureViewHandle aerialPerspective = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Aerial Perspective"), rg::TextureDef(apRes, rhi::EFragmentFormat::RGBA16_S_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderAerialPerspectivePipeline();

	RenderAerialPerspectiveConstants shaderConstants;
	shaderConstants.participatingMediaNear = apData.fogParams->nearPlane;
	shaderConstants.participatingMediaFar  = apData.fogParams->farPlane;

	lib::MTHandle<RenderAerialPerspectiveDS> ds = graphBuilder.CreateDescriptorSet<RenderAerialPerspectiveDS>(RENDERER_RESOURCE_NAME("RenderAerialPerspectiveDS"));
	ds->u_atmosphereParams            = atmosphere.atmosphereParamsBuffer->GetFullView();
	ds->u_renderAPConstants           = shaderConstants;
	ds->u_directionalLights           = atmosphere.directionalLightsBuffer->GetFullView();
	ds->u_transmittanceLUT            = atmosphere.transmittanceLUT;
	ds->u_dirLightShadowTerm          = apData.fogParams->directionalLightShadowTerm;
	ds->u_indirectInScatteringTexture = apData.fogParams->indirectInScatteringTextureView;
	ds->u_rwAerialPerspective         = aerialPerspective;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Aerial Perspective"),
						  pipeline,
						  math::Vector2u(apRes.x(), apRes.y()),
						  rg::BindDescriptorSets(std::move(ds)));

	return aerialPerspective;
}

} // aerial_perspective


AtmosphereRenderSystem::AtmosphereRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_isAtmosphereContextDirty(true)
	, m_isAtmosphereTextureDirty(false)
	, m_shouldUpdateTransmittanceLUT(true)
{
	RenderSceneRegistry& registry = owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().connect<&AtmosphereRenderSystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().connect<&AtmosphereRenderSystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().connect<&AtmosphereRenderSystem::OnDirectionalLightRemoved>(this);

	m_atmosphereParams.groundRadiusMM        = 6.360f;
	m_atmosphereParams.atmosphereRadiusMM    = 6.460f;
	m_atmosphereParams.groundAlbedo          = math::Vector3f::Constant(0.3f);
	m_atmosphereParams.rayleighScattering    = math::Vector3f(5.802f, 13.558f, 33.1f);
	m_atmosphereParams.rayleighAbsorption    = 0.0f;
	m_atmosphereParams.mieScattering         = 3.996f;
	m_atmosphereParams.mieAbsorption         = 4.4f;
	m_atmosphereParams.ozoneAbsorption       = math::Vector3f(0.650f, 1.881f, 0.085f);
	m_atmosphereParams.transmittanceAtZenith = atmosphere_utils::ComputeTransmittanceAtZenith(m_atmosphereParams);

	const math::Vector3u apResolution(32u, 32u, 64u);

	m_atmosphereParams.aerialPerspectiveParams.resolution    = apResolution;
	m_atmosphereParams.aerialPerspectiveParams.rcpResolution = apResolution.cast<Real32>().cwiseInverse();
	m_atmosphereParams.aerialPerspectiveParams.nearPlane     = 1.f;
	m_atmosphereParams.aerialPerspectiveParams.farPlane      = 8000.f;

	InitializeResources();

	m_supportedStages = rsc::ERenderStage::DeferredShading;
}

void AtmosphereRenderSystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	m_isAtmosphereTextureDirty = false;

	if (m_isAtmosphereContextDirty)
	{
		UpdateAtmosphereContext();
		m_isAtmosphereContextDirty = false;
		m_isAtmosphereTextureDirty = true;
	}
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

	const AtmosphereContext& context = GetAtmosphereContext();

	const rg::RGTextureViewHandle transmittanceLUT   = graphBuilder.AcquireExternalTextureView(context.transmittanceLUT);
	const rg::RGTextureViewHandle multiScatteringLUT = graphBuilder.AcquireExternalTextureView(context.multiScatteringLUT);

	const Bool shouldUpdateMultiScatteringLUT = m_shouldUpdateTransmittanceLUT || m_isAtmosphereTextureDirty;
	if (m_shouldUpdateTransmittanceLUT)
	{
		transmittance_lut::RenderTransmittanceLUT(graphBuilder, renderScene, context, transmittanceLUT);
		m_shouldUpdateTransmittanceLUT = false;
	}

	if (shouldUpdateMultiScatteringLUT)
	{
		multi_scattering_lut::RenderMultiScatteringLUT(graphBuilder, renderScene, context, transmittanceLUT, multiScatteringLUT);
	}

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);

		const rg::BindDescriptorSetsScope viewDSScope(graphBuilder, rg::BindDescriptorSets(viewSpec->GetRenderView().GetRenderViewDS()));

		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}

	m_volumetricCloudsRenderer.RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);
}

void AtmosphereRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const AtmosphereContext& context = GetAtmosphereContext();

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

	shadingViewContext.aerialPerspective = aerial_perspective::RenderAerialPerspective(graphBuilder, viewSpec, *this, apData);
}

void AtmosphereRenderSystem::InitializeResources()
{
	m_atmosphereContext.atmosphereParamsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Params Buffer"), rhi::BufferDefinition(rdr::shader_translator::HLSLSizeOf<AtmosphereParams>(), rhi::EBufferUsage::Uniform), rhi::EMemoryUsage::CPUToGPU);

	const auto createLUT = [](const rdr::RendererResourceName& name, math::Vector2u resolution, rhi::EFragmentFormat format)
	{
		const rhi::TextureDefinition textureDef(resolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), format);
		const lib::SharedRef<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(name, textureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		return texture->CreateView(name, viewDefinition);
	};

	m_atmosphereContext.transmittanceLUT	= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Transmittance LUT"), math::Vector2u(256u, 64u), rhi::EFragmentFormat::RGBA16_UN_Float);
	m_atmosphereContext.multiScatteringLUT	= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Multi Scattering LUT"), math::Vector2u(32u, 32u), rhi::EFragmentFormat::RGBA16_UN_Float);
}

void AtmosphereRenderSystem::UpdateAtmosphereContext()
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	const auto directionalLightsView = registry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = registry.view<const DirectionalLightData>().size();

	const Uint64 requiredBufferSize = std::max<SizeType>(directionalLightsNum, 1) * sizeof(rdr::HLSLStorage<DirectionalLightGPUData>);
	if (!m_atmosphereContext.directionalLightsBuffer || m_atmosphereContext.directionalLightsBuffer->GetSize() < requiredBufferSize)
	{
		m_atmosphereContext.directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Directional Lights Buffer"), rhi::BufferDefinition(requiredBufferSize, rhi::EBufferUsage::Storage), rhi::EMemoryUsage::CPUToGPU);
	}

	rhi::RHIMappedBuffer<rdr::HLSLStorage<DirectionalLightGPUData>> lightsGPUData(m_atmosphereContext.directionalLightsBuffer->GetRHI());

	SizeType lightIndex = 0;
	directionalLightsView.each([ & ](const DirectionalLightData& lightData, const DirectionalLightIlluminance& illuminance)
							   {
								   lightsGPUData[lightIndex++] = GPUDataBuilder::CreateDirectionalLightGPUData(lightData, illuminance);
							   });

	if (lightIndex > 0u)
	{
		const DirectionalLightData& light              = directionalLightsView.get<DirectionalLightData>(directionalLightsView.front());
		const DirectionalLightIlluminance& illuminance = directionalLightsView.get<DirectionalLightIlluminance>(directionalLightsView.front());

		m_atmosphereContext.mainDirectionalLight = GPUDataBuilder::CreateDirectionalLightGPUData(light, illuminance);
	}
	else
	{
		m_atmosphereContext.mainDirectionalLight.reset();
	}

	m_atmosphereParams.directionalLightsNum = static_cast<Uint32>(directionalLightsNum);

	rhi::RHIMappedBuffer<rdr::HLSLStorage<AtmosphereParams>> atmosphereParamsGPUData(m_atmosphereContext.atmosphereParamsBuffer->GetRHI());
	atmosphereParamsGPUData[0] = m_atmosphereParams;
}


void AtmosphereRenderSystem::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_isAtmosphereContextDirty = true;
	UpdateDirectionalLightIlluminance(entity);
}

void AtmosphereRenderSystem::OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_isAtmosphereContextDirty = true;
}

void AtmosphereRenderSystem::UpdateDirectionalLightIlluminance(RenderSceneEntity entity)
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	const DirectionalLightData& lightData = registry.get<DirectionalLightData>(entity);

	const math::Vector3f illuminanceAtZenith = lightData.color * lightData.zenithIlluminance;

	const DirectionalLightIlluminance illuminance = atmosphere_utils::ComputeDirectionalLightIlluminanceInAtmosphere(m_atmosphereParams, lightData.direction, illuminanceAtZenith, lightData.lightConeAngle);
	registry.emplace_or_replace<DirectionalLightIlluminance>(entity, illuminance);
}

} // spt::rsc
