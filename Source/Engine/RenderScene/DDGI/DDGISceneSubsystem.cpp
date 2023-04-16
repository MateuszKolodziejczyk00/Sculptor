#include "DDGISceneSubsystem.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"

namespace spt::rsc
{

DDGISceneSubsystem::DDGISceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
{
	InitializeDDGIParameters();

	InitializeTextures();
}

DDGIUpdateProbesGPUParams DDGISceneSubsystem::CreateUpdateProbesParams() const
{
	DDGIUpdateProbesGPUParams params;
	params.probesToUpdateCoords	= math::Vector3u::Zero();
	params.probesToUpdateCount	= GetProbesVolumeResolution();
	params.probeRaysMaxT		= 100.f;
	params.probeRaysMinT		= 0.03f;
	params.raysNumPerProbe		= GetRaysNumPerProbe();
	params.probesNumToUpdate	= params.probesToUpdateCount.x() * params.probesToUpdateCount.y() * params.probesToUpdateCount.z();
	params.rcpRaysNumPerProbe	= 1.f / static_cast<Real32>(params.raysNumPerProbe);
	params.rcpProbesNumToUpdate	= 1.f / static_cast<Real32>(params.probesNumToUpdate);

	return params;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesIrradianceTexture() const
{
	return m_probesIrradianceTextureView;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesHitDistanceTexture() const
{
	return m_probesHitDistanceTextureView;
}

const DDGIGPUParams& DDGISceneSubsystem::GetDDGIParams() const
{
	return m_ddgiParams;
}

EDDDGIProbesDebugType DDGISceneSubsystem::GetProbesDebugType() const
{
	return EDDDGIProbesDebugType::Irradiance;
}

Uint32 DDGISceneSubsystem::GetRaysNumPerProbe() const
{
	return 256;
}

Uint32 DDGISceneSubsystem::GetProbesNum() const
{
	return m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.y() * m_ddgiParams.probesVolumeResolution.z();
}

math::Vector3u DDGISceneSubsystem::GetProbesVolumeResolution() const
{
	return m_ddgiParams.probesVolumeResolution;
}

math::Vector2u DDGISceneSubsystem::GetProbeIrradianceDataRes() const
{
	return m_ddgiParams.probeIrradianceDataRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeIrradianceWithBorderDataRes() const
{
	return m_ddgiParams.probeIrradianceDataWithBorderRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeDistancesDataRes() const
{
	return m_ddgiParams.probeHitDistanceDataRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeDistancesDataWithBorderRes() const
{
	return m_ddgiParams.probeHitDistanceDataWithBorderRes;
}

void DDGISceneSubsystem::InitializeDDGIParameters()
{
	const math::Vector3u probesVolumeRes = math::Vector3u(20u, 20u, 10u);

	const Uint32 probesTextureWidth		= probesVolumeRes.x() * probesVolumeRes.z();
	const Uint32 probesTextureHeight	= probesVolumeRes.y();

	m_ddgiParams.probesOriginWorldLocation					= math::Vector3f(-10.f, -10.f, -0.5f);
	m_ddgiParams.probesSpacing								= math::Vector3f::Ones();
	m_ddgiParams.probesVolumeResolution						= probesVolumeRes;
	m_ddgiParams.probesWrapCoords							= math::Vector3u::Zero();

	m_ddgiParams.probeIrradianceDataRes						= math::Vector2u::Constant(6u);
	m_ddgiParams.probeIrradianceDataWithBorderRes			= m_ddgiParams.probeIrradianceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probeHitDistanceDataRes					= math::Vector2u::Constant(16u);
	m_ddgiParams.probeHitDistanceDataWithBorderRes			= m_ddgiParams.probeHitDistanceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probesIrradianceTextureRes					= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeIrradianceDataWithBorderRes);
	m_ddgiParams.probesHitDistanceTextureRes				= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes);

	m_ddgiParams.probesIrradianceTexturePixelSize			= m_ddgiParams.probesIrradianceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesIrradianceTextureUVDeltaPerProbe		= m_ddgiParams.probesIrradianceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIrradianceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesIrradianceTextureUVPerProbeNoBorder	= m_ddgiParams.probesIrradianceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIrradianceDataRes.cast<Real32>());
	
	m_ddgiParams.probesHitDistanceTexturePixelSize			= m_ddgiParams.probesHitDistanceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesHitDistanceUVDeltaPerProbe			= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesHitDistanceTextureUVPerProbeNoBorder	= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataRes.cast<Real32>());
}

void DDGISceneSubsystem::InitializeTextures()
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesTextureWidth		= m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.z();
	const Uint32 probesTextureHeight	= m_ddgiParams.probesVolumeResolution.y();

	const math::Vector2u irradiancePerProbeRes	= GetProbeIrradianceWithBorderDataRes();
	const math::Vector2u distancePerProbeRes	= GetProbeDistancesDataWithBorderRes();

	const math::Vector3u probesRes = GetProbesVolumeResolution();

	const rhi::ETextureUsage texturesUsage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

	rhi::TextureDefinition probesIrradianceTextureDef;
	probesIrradianceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * irradiancePerProbeRes.x(), probesTextureHeight * irradiancePerProbeRes.y(), 1u };
	probesIrradianceTextureDef.usage		= texturesUsage;
	probesIrradianceTextureDef.format		= rhi::EFragmentFormat::B10G11B11_U_Float;
	const lib::SharedRef<rdr::Texture> probesIrradianceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Irradiance"), probesIrradianceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesIrradianceViewDefinition;
	probesIrradianceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesIrradianceTextureView = probesIrradianceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Irradiance View"), probesIrradianceViewDefinition);

	rhi::TextureDefinition probesDistanceTextureDef;
	probesDistanceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * distancePerProbeRes.x(), probesTextureHeight * distancePerProbeRes.y(), 1u };
	probesDistanceTextureDef.usage		= texturesUsage;
	probesDistanceTextureDef.format		= rhi::EFragmentFormat::RG16_UN_Float;
	const lib::SharedRef<rdr::Texture> probesDistanceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Distance"), probesDistanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesDistanceViewDefinition;
	probesDistanceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesHitDistanceTextureView = probesDistanceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Distance View"), probesDistanceViewDefinition);
}

} // spt::rsc
