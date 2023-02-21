#include "ShadowMapsManagerSystem.h"
#include "RHICore/RHITextureTypes.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "SceneRenderer/RenderStages/ShadowMapRenderStage.h"
#include "RenderScene.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Lights/LightTypes.h"
#include "ShadowsRenderingTypes.h"

namespace spt::rsc
{

namespace params
{

RendererIntParameter maxShadowMapsUpgradedPerFrame("Max ShadowMaps Upgraded Per Frame", { "Lighting", "Shadows" }, 3, 0, 10);

} // params

ShadowMapsManagerSystem::ShadowMapsManagerSystem(RenderScene& owningScene)
	: Super(owningScene)
	, highQualityShadowMapsMaxIdx(0)
	, mediumQualityShadowMapsMaxIdx(0)
{
	CreateShadowMaps();
}

void ShadowMapsManagerSystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	lib::DynamicArray<lib::UniquePtr<RenderView>> shadowMapViws;
	for (const RenderSceneEntity lightEntity : GetPointLightsWithShadowMapsToUpdate())
	{
		const PointLightData& pointLightData					= sceneRegistry.get<PointLightData>(lightEntity);
		const PointLightShadowMapComponent& pointLightShadowMap	= sceneRegistry.get<PointLightShadowMapComponent>(lightEntity);

		UpdateShadowMapRenderViews(lightEntity, pointLightData, pointLightShadowMap.shadowMapFirstFaceIdx);
	}
}

Bool ShadowMapsManagerSystem::CanRenderShadows() const
{
	return !m_shadowMapViews.empty();
}

void ShadowMapsManagerSystem::UpdateVisibleLights(lib::DynamicArray<VisibleLightEntityInfo>& visibleLights)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(CanRenderShadows());
	SPT_CHECK(m_shadowMapViews.size() % 6 == 0);

	m_lightsWithUpdatedShadowMaps.clear();

	const SizeType availableShadowMapsNum = m_shadowMapViews.size() / 6;
	const SizeType shadowMapsInUse = std::min(availableShadowMapsNum, visibleLights.size());
		
	const auto compareOp = [](const VisibleLightEntityInfo& lhs, const VisibleLightEntityInfo& rhs) { return lhs.areaOnScreen > rhs.areaOnScreen; };

	std::nth_element(std::begin(visibleLights), std::begin(visibleLights) + shadowMapsInUse, std::end(visibleLights), compareOp);
	std::sort(std::begin(visibleLights), std::begin(visibleLights) + shadowMapsInUse, compareOp);

	lib::DynamicArray<std::pair<RenderSceneEntity, EShadowMapQuality>> shadowMapsToUpgrade;

	struct ShadowMapReleaseInfo
	{
		RenderSceneEntity entity;
		EShadowMapQuality desiredQualityToAcquire;
	};
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<ShadowMapReleaseInfo>> shadowMapsToDowngrade;

	const lib::HashMap<RenderSceneEntity, EShadowMapQuality> pointLightsWithAssignedShadowMapsPrevFrame = std::move(m_pointLightsWithAssignedShadowMaps);

	lib::HashSet<RenderSceneEntity> lightsVisibleCurrentFrame;
	lightsVisibleCurrentFrame.reserve(shadowMapsInUse);

	for (SizeType lightIdx = 0; lightIdx < shadowMapsInUse; ++lightIdx)
	{
		const RenderSceneEntity lightEntity = visibleLights[lightIdx].entity;

		lightsVisibleCurrentFrame.emplace(lightEntity);

		const auto foundCurrentQuality = pointLightsWithAssignedShadowMapsPrevFrame.find(lightEntity);
		const EShadowMapQuality currentQuality = foundCurrentQuality != std::cend(pointLightsWithAssignedShadowMapsPrevFrame) ? foundCurrentQuality->second : EShadowMapQuality::None;
		const EShadowMapQuality desiredQuality = GetShadowMapQuality(lightIdx);

		if (static_cast<Uint32>(currentQuality) < static_cast<Uint32>(desiredQuality))
		{
			if (shadowMapsToUpgrade.size() < static_cast<SizeType>(params::maxShadowMapsUpgradedPerFrame.GetValue()))
			{
				if (currentQuality != EShadowMapQuality::None)
				{
					ReleaseShadowMap(currentQuality, ResetPointLightShadowMap(lightEntity));
				}

				shadowMapsToUpgrade.emplace_back(std::make_pair(lightEntity, desiredQuality));
			}
			else
			{
				m_pointLightsWithAssignedShadowMaps[lightEntity] = currentQuality;
			}

		}
		else if (static_cast<Uint32>(currentQuality) > static_cast<Uint32>(desiredQuality))
		{
			shadowMapsToDowngrade[currentQuality].emplace_back(ShadowMapReleaseInfo{ lightEntity, desiredQuality });
		}
		else
		{
			m_pointLightsWithAssignedShadowMaps[lightEntity] = desiredQuality;
		}
	}

	for (const auto [entity, quality] : pointLightsWithAssignedShadowMapsPrevFrame)
	{
		if (!lightsVisibleCurrentFrame.contains(entity))
		{
			ReleaseShadowMap(quality, ResetPointLightShadowMap(entity));
		}
	}

	std::reverse(std::begin(shadowMapsToUpgrade), std::end(shadowMapsToUpgrade));

	while (!shadowMapsToUpgrade.empty())
	{
		const auto [acquireLightEntity, acquireDesiredQuality] = shadowMapsToUpgrade.back();
		shadowMapsToUpgrade.pop_back();

		Uint32 shadowMapIdx = AcquireAvaialableShadowMap(acquireDesiredQuality);
		if (shadowMapIdx == idxNone<Uint32>)
		{
			lib::DynamicArray<ShadowMapReleaseInfo>& shadowMapsOfQualityToRelease = shadowMapsToDowngrade[acquireDesiredQuality];
			const ShadowMapReleaseInfo releaseInfo = shadowMapsOfQualityToRelease.back();
			shadowMapsOfQualityToRelease.pop_back();

			shadowMapIdx = ResetPointLightShadowMap(releaseInfo.entity);

			if (releaseInfo.desiredQualityToAcquire != EShadowMapQuality::None)
			{
				shadowMapsToUpgrade.emplace_back(std::make_pair(releaseInfo.entity, releaseInfo.desiredQualityToAcquire));
			}
		}
	
		SetPointLightShadowMapsBeginIdx(acquireLightEntity, shadowMapIdx);
		m_pointLightsWithAssignedShadowMaps[acquireLightEntity] = acquireDesiredQuality;
	}

	for (const auto [releasedQuality, releaseInfos] : shadowMapsToDowngrade)
	{
		for (const ShadowMapReleaseInfo& releaseInfo : releaseInfos)
		{
			if (lightsVisibleCurrentFrame.contains(releaseInfo.entity))
			{
				m_pointLightsWithAssignedShadowMaps[releaseInfo.entity] = releasedQuality;
			}
		}
	}
}

const lib::DynamicArray<RenderSceneEntity>& ShadowMapsManagerSystem::GetPointLightsWithShadowMapsToUpdate() const
{
	return m_lightsWithUpdatedShadowMaps;
}

lib::DynamicArray<RenderView*> ShadowMapsManagerSystem::GetShadowMapViewsToUpdate() const
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	const lib::DynamicArray<RenderSceneEntity>& lightsToUpdate = GetPointLightsWithShadowMapsToUpdate();

	lib::DynamicArray<RenderView*> renderViewsToUpdate;
	renderViewsToUpdate.reserve(lightsToUpdate.size() * 6);

	for (RenderSceneEntity light : lightsToUpdate)
	{
		const PointLightShadowMapComponent& pointLightShadowMap	= sceneRegistry.get<PointLightShadowMapComponent>(light);
		
		for (SizeType faceIdx = 0; faceIdx < 6; ++faceIdx)
		{
			renderViewsToUpdate.emplace_back(m_shadowMapsRenderViews[static_cast<SizeType>(pointLightShadowMap.shadowMapFirstFaceIdx) + faceIdx].get());
		}
	}

	return renderViewsToUpdate;
}

lib::DynamicArray<RenderView*> ShadowMapsManagerSystem::GetPointLightShadowMapViews(const PointLightShadowMapComponent& pointLightShadowMap) const
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<RenderView*> views;
	views.reserve(6);

	for (SizeType faceIdx = 0; faceIdx < 6; ++faceIdx)
	{
		views.emplace_back(m_shadowMapsRenderViews[static_cast<SizeType>(pointLightShadowMap.shadowMapFirstFaceIdx) + faceIdx].get());
	}

	return views;
}

void ShadowMapsManagerSystem::SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx)
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();
	registry.emplace<PointLightShadowMapComponent>(pointLightEntity, PointLightShadowMapComponent{ shadowMapBeginIdx });
	m_lightsWithUpdatedShadowMaps.emplace_back(pointLightEntity);
}

Uint32 ShadowMapsManagerSystem::ResetPointLightShadowMap(RenderSceneEntity pointLightEntity) const
{
	Uint32 shadowMapFirstFaceIdx = idxNone<Uint32>;

	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();
	const PointLightShadowMapComponent* shadowMapComp = registry.try_get<PointLightShadowMapComponent>(pointLightEntity);
	if (shadowMapComp)
	{
		shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
		registry.remove<PointLightShadowMapComponent>(pointLightEntity);
	}
	return shadowMapFirstFaceIdx;
}

EShadowMapQuality ShadowMapsManagerSystem::GetShadowMapQuality(SizeType pointLightIdx) const
{
	if (pointLightIdx < highQualityShadowMapsMaxIdx)
	{
		return EShadowMapQuality::High;
	}
	else if (pointLightIdx < mediumQualityShadowMapsMaxIdx)
	{
		return EShadowMapQuality::Medium;
	}

	return EShadowMapQuality::Low;
}

void ShadowMapsManagerSystem::ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx)
{
	m_availableShadowMaps[quality].emplace_back(shadowMapIdx);
}

Uint32 ShadowMapsManagerSystem::AcquireAvaialableShadowMap(EShadowMapQuality quality)
{
	Uint32 shadowMapIdx = idxNone<Uint32>;

	lib::DynamicArray<Uint32>& availableIndices = m_availableShadowMaps.at(quality);
	if (!availableIndices.empty())
	{
		shadowMapIdx = availableIndices.back();
		availableIndices.pop_back();
	}

	return shadowMapIdx;
}

void ShadowMapsManagerSystem::CreateShadowMaps()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_shadowMapViews.empty());

	const rhi::EFragmentFormat shadowMapsFormat = ShadowMapRenderStage::GetDepthFormat();

	const auto CreateShadowMapsHelper = [this, shadowMapsFormat](math::Vector2u resolution, SizeType shadowMapsNum, EShadowMapQuality quality)
	{
		const Uint32 prevShadowMapsNum = static_cast<Uint32>(m_shadowMapViews.size());

		const SizeType shadowMapTexturesToCreate = shadowMapsNum * 6;
		m_shadowMapViews.reserve(m_shadowMapViews.size() + shadowMapTexturesToCreate);
		for (Uint32 i = 0; i < shadowMapTexturesToCreate; ++i)
		{
			rhi::TextureDefinition textureDef;
			textureDef.resolution.head<2>()	= resolution;
			textureDef.resolution.z()		= 1;
			textureDef.usage				= lib::Flags(rhi::ETextureUsage::DepthSetncilRT, rhi::ETextureUsage::SampledTexture);
			textureDef.format				= shadowMapsFormat;
			const lib::SharedRef<rdr::Texture> shadowMap = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Shadow Map"), textureDef, rhi::EMemoryUsage::GPUOnly);
	
			rhi::TextureViewDefinition shadowMapViewDef;
			shadowMapViewDef.subresourceRange.aspect = rhi::ETextureAspect::Depth;
			m_shadowMapViews.emplace_back(shadowMap->CreateView(RENDERER_RESOURCE_NAME("Shadow Map View"), shadowMapViewDef));
		}

		lib::DynamicArray<Uint32>& shadowMapsOfQuality = m_availableShadowMaps[quality];
		for (SizeType idx = prevShadowMapsNum; idx < m_shadowMapViews.size(); idx += 6)
		{
			shadowMapsOfQuality.emplace_back(static_cast<Uint32>(idx));
		}
	};

	const SizeType highQualityShadowMaps	= 8;
	const SizeType mediumQualityShadowMaps	= 16;
	const SizeType lowQualityShadowMaps		= 64;

	CreateShadowMapsHelper(math::Vector2u::Constant(1024),	highQualityShadowMaps,		EShadowMapQuality::High);
	CreateShadowMapsHelper(math::Vector2u::Constant(512),	mediumQualityShadowMaps,	EShadowMapQuality::Medium);
	CreateShadowMapsHelper(math::Vector2u::Constant(256),	lowQualityShadowMaps,		EShadowMapQuality::Low);

	highQualityShadowMapsMaxIdx		= static_cast<Uint32>(highQualityShadowMaps);
	mediumQualityShadowMapsMaxIdx	= static_cast<Uint32>(highQualityShadowMaps + mediumQualityShadowMaps);

	CreateShadowMapsRenderViews();
}

void ShadowMapsManagerSystem::CreateShadowMapsRenderViews()
{
	SPT_PROFILER_FUNCTION();
	
	m_shadowMapsRenderViews.reserve(m_shadowMapViews.size());

	for (SizeType idx = 0; idx < m_shadowMapViews.size(); ++idx)
	{
		lib::UniquePtr<RenderView>& renderView = m_shadowMapsRenderViews.emplace_back();
		renderView = std::make_unique<RenderView>(GetOwningScene());
		renderView->SetRenderStages(ERenderStage::ShadowMap);
	}
}

void ShadowMapsManagerSystem::UpdateShadowMapRenderViews(RenderSceneEntity owningLight, const PointLightData& pointLight, Uint32 shadowMapBeginIdx)
{
	SPT_PROFILER_FUNCTION();

	const lib::StaticArray<math::Quaternionf, 6> faceRotations =
	{
		math::Utils::EulerToQuaternionDegrees(0.f,		0.f,	0.f),	// +X
		math::Utils::EulerToQuaternionDegrees(0.f,		0.f,	180.f),	// -X
		math::Utils::EulerToQuaternionDegrees(0.f,		0.f,	90.f),	// +Y
		math::Utils::EulerToQuaternionDegrees(0.f,		0.f,	-90.f),	// -Y
		math::Utils::EulerToQuaternionDegrees(0.f,		90.f,	0.f),	// +Z
		math::Utils::EulerToQuaternionDegrees(0.f,		-90.f,	0.f),	// -Z
	};

	for (SizeType faceIdx = 0; faceIdx < faceRotations.size(); ++faceIdx)
	{
		const SizeType renderViewIdx = static_cast<SizeType>(shadowMapBeginIdx) + faceIdx;

		const lib::UniquePtr<RenderView>& renderView = m_shadowMapsRenderViews[renderViewIdx];
		const RenderSceneEntityHandle renderViewEntity = renderView->GetViewEntity();

		const lib::SharedRef<rdr::TextureView>& shadowMapTextureView = m_shadowMapViews[renderViewIdx];
		const math::Vector2u shadowMapResolution = shadowMapTextureView->GetTexture()->GetResolution2D();

		ShadowMapViewComponent shadowMapViewComp;
		shadowMapViewComp.shadowMapView	= shadowMapTextureView;
		shadowMapViewComp.owningLight	= owningLight;
		shadowMapViewComp.faceIdx		= static_cast<Uint32>(faceIdx);
		renderViewEntity.emplace_or_replace<ShadowMapViewComponent>(shadowMapViewComp);

		constexpr Real32 nearPlane = 0.01f;
		renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(90.f), 1.f, nearPlane, pointLight.radius);
		renderView->SetLocation(pointLight.location);
		renderView->SetRotation(faceRotations[faceIdx]);

		renderView->SetRenderingResolution(shadowMapResolution);
	}
}

} // spt::rsc
