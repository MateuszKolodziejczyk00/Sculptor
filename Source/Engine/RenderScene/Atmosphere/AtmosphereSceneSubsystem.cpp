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
{
	RenderSceneRegistry& registry = owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().connect<&AtmosphereSceneSubsystem::OnDirectionalLightUpdated>(this);

	m_atmosphereParams.groundRadiusMM		= 6.360f;
	m_atmosphereParams.atmosphereRadiusMM	= 6.460f;

	m_atmosphereParams.groundAlbedo			= math::Vector3f::Constant(0.3f);

	m_atmosphereParams.rayleighScattering	= math::Vector3f(5.802f, 13.558f, 33.1f);
	m_atmosphereParams.rayleighAbsorption	= 0.0f;
	
	m_atmosphereParams.mieScattering		= 3.996f;
	m_atmosphereParams.mieAbsorption		= 4.4f;

	m_atmosphereParams.ozoneAbsorption		= math::Vector3f(0.650f, 1.881f, 0.085f);

	InitializeResources();
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

	m_atmosphereContext.transmittanceLUT	= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Transmittance LUT"), math::Vector2u(256u, 64u), rhi::EFragmentFormat::B10G11R11_U_Float);
	m_atmosphereContext.multiScatteringLUT	= createLUT(RENDERER_RESOURCE_NAME("Atmosphere Multi Scattering LUT"), math::Vector2u(32u, 32u), rhi::EFragmentFormat::B10G11R11_U_Float);
}

void AtmosphereSceneSubsystem::UpdateAtmosphereContext()
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	const auto directionalLightsView = registry.view<DirectionalLightData>();
	const SizeType directionalLightsNum = directionalLightsView.size();

	const Uint64 requiredBufferSize = directionalLightsNum * sizeof(DirectionalLightGPUData);
	if (!m_atmosphereContext.directionalLightsBuffer || m_atmosphereContext.directionalLightsBuffer->GetSize() < requiredBufferSize)
	{
		m_atmosphereContext.directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Atmosphere Directional Lights Buffer"), rhi::BufferDefinition(requiredBufferSize, rhi::EBufferUsage::Storage), rhi::EMemoryUsage::CPUToGPU);
	}

	rhi::RHIMappedBuffer<DirectionalLightGPUData> lightsGPUData(m_atmosphereContext.directionalLightsBuffer->GetRHI());

	SizeType lightIndex = 0;
	directionalLightsView.each([ & ](const DirectionalLightData& lightData)
							   {
								   lightsGPUData[lightIndex++] = lightData.GenerateGPUData();
							   });

	m_atmosphereParams.directionalLightsNum = static_cast<Uint32>(directionalLightsNum);

	rhi::RHIMappedBuffer<AtmosphereParams> atmosphereParamsGPUData(m_atmosphereContext.atmosphereParamsBuffer->GetRHI());
	atmosphereParamsGPUData[0] = m_atmosphereParams;
}

void AtmosphereSceneSubsystem::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_isAtmosphereContextDirty = true;
}

} // spt::rsc
