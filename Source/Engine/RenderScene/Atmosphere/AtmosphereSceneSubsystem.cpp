#include "AtmosphereSceneSubsystem.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

AtmosphereSceneSubsystem::AtmosphereSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_isAtmosphereContextDirty(true)
	, m_isAtmosphereTextureDirty(false)
	, m_shouldUpdateTransmittanceLUT(true)
{
	RenderSceneRegistry& registry = owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightRemoved>(this);

	m_atmosphereParams.groundRadiusMM        = 6.360f;
	m_atmosphereParams.atmosphereRadiusMM    = 6.460f;
	m_atmosphereParams.groundAlbedo          = math::Vector3f::Constant(0.3f);
	m_atmosphereParams.rayleighScattering    = math::Vector3f(5.802f, 13.558f, 33.1f);
	m_atmosphereParams.rayleighAbsorption    = 0.0f;
	m_atmosphereParams.mieScattering         = 3.996f;
	m_atmosphereParams.mieAbsorption         = 4.4f;
	m_atmosphereParams.ozoneAbsorption       = math::Vector3f(0.650f, 1.881f, 0.085f);
	m_atmosphereParams.transmittanceAtZenith = atmosphere_utils::ComputeTransmittanceAtZenith(m_atmosphereParams);

	InitializeResources();
}

AtmosphereSceneSubsystem::~AtmosphereSceneSubsystem()
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	registry.on_construct<DirectionalLightData>().disconnect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().disconnect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().disconnect<&AtmosphereSceneSubsystem::OnDirectionalLightRemoved>(this);
}

void AtmosphereSceneSubsystem::Update()
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

const AtmosphereContext& AtmosphereSceneSubsystem::GetAtmosphereContext() const
{
	return m_atmosphereContext;
}

Bool AtmosphereSceneSubsystem::IsAtmosphereTextureDirty() const
{
	return m_isAtmosphereTextureDirty;
}

Bool AtmosphereSceneSubsystem::ShouldUpdateTransmittanceLUT() const
{
	return m_shouldUpdateTransmittanceLUT;
}

void AtmosphereSceneSubsystem::PostUpdateTransmittanceLUT()
{
	m_shouldUpdateTransmittanceLUT = false;
}


void AtmosphereSceneSubsystem::InitializeResources()
{
	m_atmosphereContext.atmosphereParamsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Params Buffer"), rhi::BufferDefinition(sizeof(AtmosphereParams), rhi::EBufferUsage::Uniform), rhi::EMemoryUsage::CPUToGPU);

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

void AtmosphereSceneSubsystem::UpdateAtmosphereContext()
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	const auto directionalLightsView = registry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = registry.view<const DirectionalLightData>().size();

	const Uint64 requiredBufferSize = std::max<SizeType>(directionalLightsNum, 1) * sizeof(DirectionalLightGPUData);
	if (!m_atmosphereContext.directionalLightsBuffer || m_atmosphereContext.directionalLightsBuffer->GetSize() < requiredBufferSize)
	{
		m_atmosphereContext.directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Directional Lights Buffer"), rhi::BufferDefinition(requiredBufferSize, rhi::EBufferUsage::Storage), rhi::EMemoryUsage::CPUToGPU);
	}

	rhi::RHIMappedBuffer<DirectionalLightGPUData> lightsGPUData(m_atmosphereContext.directionalLightsBuffer->GetRHI());

	SizeType lightIndex = 0;
	directionalLightsView.each([ & ](const DirectionalLightData& lightData, const DirectionalLightIlluminance& illuminance)
							   {
								   lightsGPUData[lightIndex++] = GPUDataBuilder::CreateDirectionalLightGPUData(lightData, illuminance);
							   });

	m_atmosphereParams.directionalLightsNum = static_cast<Uint32>(directionalLightsNum);

	rhi::RHIMappedBuffer<AtmosphereParams> atmosphereParamsGPUData(m_atmosphereContext.atmosphereParamsBuffer->GetRHI());
	atmosphereParamsGPUData[0] = m_atmosphereParams;
}


void AtmosphereSceneSubsystem::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_isAtmosphereContextDirty = true;
	UpdateDirectionalLightIlluminance(entity);
}

void AtmosphereSceneSubsystem::OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_isAtmosphereContextDirty = true;
}

void AtmosphereSceneSubsystem::UpdateDirectionalLightIlluminance(RenderSceneEntity entity)
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	const DirectionalLightData& lightData = registry.get<DirectionalLightData>(entity);

	const math::Vector3f illuminanceAtZenith = lightData.color * lightData.zenithIlluminance;

	const DirectionalLightIlluminance illuminance = atmosphere_utils::ComputeDirectionalLightIlluminanceInAtmosphere(m_atmosphereParams, lightData.direction, illuminanceAtZenith, lightData.lightConeAngle);
	registry.emplace_or_replace<DirectionalLightIlluminance>(entity, illuminance);
}

} // spt::rsc
