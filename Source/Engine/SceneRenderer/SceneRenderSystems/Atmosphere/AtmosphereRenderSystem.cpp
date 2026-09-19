#include "AtmosphereRenderSystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "RenderSceneConstants.h"
#include "ResourcesManager.h"
#include "Lights/LightTypes.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"


namespace spt::rsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(AtmosphereRenderSystem);


namespace transmittance_lut
{

BEGIN_SHADER_STRUCT(RenderTransmittanceLUTConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwTransmittanceLUT)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,     atmosphereParams)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileRenderTransmittanceLUTPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/TransmittanceLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderTransmittanceLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderTransmittanceLUTPipeline"), shader);
}


static void RenderTransmittanceLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittanceLUT)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileRenderTransmittanceLUTPipeline();
	
	RenderTransmittanceLUTConstants shaderConstants;
	shaderConstants.rwTransmittanceLUT = transmittanceLUT;
	shaderConstants.atmosphereParams   = context.atmosphereParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Transmittance LUT"),
						  pipeline,
						  math::Utils::DivideCeil(transmittanceLUT->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
}

} // transmittance_lut

namespace multi_scattering_lut
{

BEGIN_SHADER_STRUCT(RenderMultiScatteringLUTConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwMultiScatteringLUT)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,     atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, transmittanceLUT)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileRenderMultiScatteringLUTPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/MultiScatteringLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderMultiScatteringLUTCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderMultiScatteringLUTPipeline"), shader);
}


static void RenderMultiScatteringLUT(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const AtmosphereContext& context, rg::RGTextureViewHandle transmittancleLUT, rg::RGTextureViewHandle multiScatteringLUT)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileRenderMultiScatteringLUTPipeline();

	RenderMultiScatteringLUTConstants shaderConstants;
	shaderConstants.rwMultiScatteringLUT = multiScatteringLUT;
	shaderConstants.atmosphereParams     = context.atmosphereParams;
	shaderConstants.transmittanceLUT     = transmittancleLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Multi Scattering LUT"),
						  pipeline,
						  math::Utils::DivideCeil(multiScatteringLUT->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
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


BEGIN_SHADER_STRUCT(SkyViewConstants)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,                atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DirectionalLightGPUData>, directionalLights)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DirectionalLightGPUData>, directionalLightsPtr)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>,         transmittanceLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>,         multiScatteringLUT)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>,         skyViewLUT)
END_SHADER_STRUCT();


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

	SkyViewConstants shaderConstants;
	shaderConstants.atmosphereParams     = context.atmosphereParams;
	shaderConstants.directionalLights    = context.directionalLightsBuffer->GetFullView();
	shaderConstants.directionalLightsPtr.Set(context.directionalLightsBuffer->GetFullView());
	shaderConstants.transmittanceLUT     = skyViewParams.transmittanceLUT;
	shaderConstants.multiScatteringLUT   = skyViewParams.multiScatteringLUT;
	shaderConstants.skyViewLUT           = skyViewLUT;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Atmosphere Sky View LUT"),
						  pipeline,
						  math::Utils::DivideCeil(skyViewLUTResolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));

	return skyViewLUT;
}

} // sky_view


namespace render_sky_probe
{

BEGIN_SHADER_STRUCT(RenderSkyProbeConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, skyViewLUT)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwProbe)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,     atmosphereParams)
END_SHADER_STRUCT();


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

	RenderSkyProbeConstants shaderConstants;
	shaderConstants.skyViewLUT       = skyViewLUT;
	shaderConstants.rwProbe          = skyProbe;
	shaderConstants.atmosphereParams = atmosphere.atmosphereParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Sky Probe"),
						  pipeline,
						  math::Vector2u(1u, 1u),
						  rg::ShaderParams(shaderConstants));
												

	return skyProbe;
}

} // render_sky_probe


namespace aerial_perspective
{

BEGIN_SHADER_STRUCT(RenderAerialPerspectiveConstants)
	SHADER_STRUCT_FIELD(Real32,                                    participatingMediaNear)
	SHADER_STRUCT_FIELD(Real32,                                    participatingMediaFar)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,             atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DirectionalLightGPUData>, directionalLights)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,         transmittanceLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>,                 dirLightShadowTerm)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector3f>,         indirectInScatteringTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>,         rwAerialPerspective)
END_SHADER_STRUCT();


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
	shaderConstants.atmosphereParams            = atmosphere.atmosphereParams;
	shaderConstants.directionalLights           = atmosphere.directionalLightsBuffer->GetFullView();
	shaderConstants.transmittanceLUT            = atmosphere.transmittanceLUT;
	shaderConstants.dirLightShadowTerm          = apData.fogParams->directionalLightShadowTerm;
	shaderConstants.indirectInScatteringTexture = apData.fogParams->indirectInScatteringTextureView;
	shaderConstants.rwAerialPerspective         = aerialPerspective;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Aerial Perspective"),
						  pipeline,
						  math::Vector2u(apRes.x(), apRes.y()),
						  rg::ShaderParams(shaderConstants));

	return aerialPerspective;
}

} // aerial_perspective


AtmosphereRenderSystem::AtmosphereRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
	, m_isAtmosphereTextureDirty(false)
	, m_shouldUpdateTransmittanceLUT(true)
{
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

void AtmosphereRenderSystem::Update(const SceneUpdateContext& context)
{
	SPT_PROFILER_FUNCTION();

	Super::Update(context);

	m_isAtmosphereTextureDirty = false;

	if (GetOwningScene().lighting.IsDirectionalLightDirty())
	{
		UpdateDirectionalLightIlluminance(GetOwningScene().lighting.GetDirectionalLight());
		UpdateAtmosphereContext();
		m_isAtmosphereTextureDirty = true;
	}
}

void AtmosphereRenderSystem::UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData)
{
	Super::UpdateGPUSceneData(context, sceneData);

	const AtmosphereContext& atmosphereContext = GetAtmosphereContext();

	SceneAtmosphereData atmosphereData;
	atmosphereData.atmosphereParams   = m_atmosphereParams;
	atmosphereData.transmittanceLUT   = atmosphereContext.transmittanceLUT;
	atmosphereData.multiScatteringLUT = atmosphereContext.multiScatteringLUT;

	if (atmosphereContext.mainDirectionalLight)
	{
		atmosphereData.mainDirectionalLight = *atmosphereContext.mainDirectionalLight;
	}

	sceneData.atmosphere = atmosphereData;
}

void AtmosphereRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);

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

		const rg::BindShaderParamsScope viewParamsScope(graphBuilder, rg::ShaderParams(viewSpec->GetViewShaderParams()));

		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}

	m_volumetricCloudsRenderer.RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);
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

	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderAerialPerspective).AddRawMember(this, &AtmosphereRenderSystem::RenderAerialPerspective);
}

void AtmosphereRenderSystem::RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const
{
	SPT_PROFILER_FUNCTION();

	const RenderViewEntryDelegates::RenderAerialPerspectiveData& apData = context.Get<RenderViewEntryDelegates::RenderAerialPerspectiveData>();

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	shadingViewContext.aerialPerspective = aerial_perspective::RenderAerialPerspective(graphBuilder, viewSpec, *this, apData);
}

void AtmosphereRenderSystem::InitializeResources()
{
	lib::SharedRef<rdr::Buffer> atmosphereParamsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Params Buffer"), rhi::BufferDefinition(rdr::shader_translator::HLSLSizeOf<AtmosphereParams>(), rhi::EBufferUsage::Uniform), rhi::EMemoryUsage::CPUToGPU);
	m_atmosphereContext.atmosphereParams.Set(atmosphereParamsBuffer->GetFullView(), 0u);

	const auto createLUT = [](const rdr::RendererResourceName& name, math::Vector2u resolution, rhi::EFragmentFormat format)
	{
		rhi::TextureDefinition textureDef(resolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), format);
		textureDef.flags = rhi::ETextureFlags::GloballyReadable;
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
	const Uint64 requiredBufferSize = sizeof(rdr::HLSLStorage<DirectionalLightGPUData>);
	if (!m_atmosphereContext.directionalLightsBuffer || m_atmosphereContext.directionalLightsBuffer->GetSize() < requiredBufferSize)
	{
		m_atmosphereContext.directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Directional Light Buffer"), rhi::BufferDefinition(requiredBufferSize, rhi::EBufferUsage::Storage), rhi::EMemoryUsage::CPUToGPU);
	}

	rhi::RHIMappedBuffer<rdr::HLSLStorage<DirectionalLightGPUData>> lightGPUData(m_atmosphereContext.directionalLightsBuffer->GetRHI());

	const DirectionalLightGPUData directionalLightGPUData = GPUDataBuilder::CreateDirectionalLightGPUData(GetOwningScene().lighting.GetDirectionalLight(), m_directionalLightIlluminance);
	lightGPUData[0] = directionalLightGPUData;

	m_atmosphereContext.mainDirectionalLight = directionalLightGPUData;

	m_atmosphereParams.directionalLightsNum = 1u;

	m_atmosphereContext.atmosphereParams.SetData(m_atmosphereParams);
}

void AtmosphereRenderSystem::UpdateDirectionalLightIlluminance(const DirectionalLightData& dirLight)
{
	const math::Vector3f illuminanceAtZenith = dirLight.color * dirLight.zenithIlluminance;

	m_directionalLightIlluminance = atmosphere_utils::ComputeDirectionalLightIlluminanceInAtmosphere(m_atmosphereParams, dirLight.direction, illuminanceAtZenith, dirLight.lightConeAngle);
}

} // spt::rsc
