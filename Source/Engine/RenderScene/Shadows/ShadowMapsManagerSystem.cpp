#include "ShadowMapsManagerSystem.h"
#include "RHICore/RHITextureTypes.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "SceneRenderer/RenderStages/ShadowMapRenderStage.h"
#include "RenderScene.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Lights/LightTypes.h"
#include "ShadowsRenderingTypes.h"
#include "EngineTimer.h"
#include "Engine.h"
#include "YAMLSerializerHelper.h"
#include "ConfigUtils.h"
#include "RendererSettings.h"
#include "Transfers/UploadUtils.h"

namespace spt::rsc
{

struct ShadowMapsSettings
{
	ShadowMapsSettings()
		: highQualityShadowMaps(0)
		, mediumQualityShadowMaps(0)
		, lowQualityShadowMaps(0)
	{ }

	Uint32 highQualityShadowMaps;
	Uint32 mediumQualityShadowMaps;
	Uint32 lowQualityShadowMaps;
};

} // spt::rsc

namespace spt::srl
{

template<>
struct TypeSerializer<rsc::ShadowMapsSettings>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("HighQualityShadowMaps", data.highQualityShadowMaps);
		serializer.Serialize("MediumQualityShadowMaps", data.mediumQualityShadowMaps);
		serializer.Serialize("LowQualityShadowMaps", data.lowQualityShadowMaps);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::rsc::ShadowMapsSettings)


namespace spt::rsc
{

namespace params
{

RendererIntParameter maxShadowMapsUpgradedPerFrame("Max ShadowMaps Upgraded Per Frame", { "Lighting", "Shadows" }, 1, 0, 10);
RendererIntParameter maxShadowMapsUpdatedPerFrame("Max ShadowMaps Updated Per Frame", { "Lighting", "Shadows" }, 3, 0, 10);

} // params


ShadowMapsManagerSystem::ShadowMapsManagerSystem(RenderScene& owningScene)
	: Super(owningScene)
	, highQualityShadowMapsMaxIdx(0)
	, mediumQualityShadowMapsMaxIdx(0)
{
	CreateShadowMaps();

	CreateShadowMapsDescriptorSet();
}

void ShadowMapsManagerSystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	UpdateShadowMaps();

	RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	lib::DynamicArray<lib::UniquePtr<RenderView>> shadowMapViws;

	const lib::DynamicArray<RenderSceneEntity>& pointLightsWithShadowMapsToUpdate = GetPointLightsWithShadowMapsToUpdate();
	for (const RenderSceneEntity lightEntity : pointLightsWithShadowMapsToUpdate)
	{
		const PointLightData& pointLightData					= sceneRegistry.get<PointLightData>(lightEntity);
		const PointLightShadowMapComponent& pointLightShadowMap	= sceneRegistry.get<PointLightShadowMapComponent>(lightEntity);

		UpdateShadowMapRenderViews(lightEntity, pointLightData, pointLightShadowMap.shadowMapFirstFaceIdx);
	}

	if (!pointLightsWithShadowMapsToUpdate.empty())
	{
		UpdateShadowMapsDSViewsData();
	}
}

Bool ShadowMapsManagerSystem::CanRenderShadows() const
{
	return !m_shadowMaps.empty();
}

const lib::SharedPtr<ShadowMapsDS>& ShadowMapsManagerSystem::GetShadowMapsDS() const
{
	return m_shadowMapsDS;
}

void ShadowMapsManagerSystem::UpdateVisibleLights(lib::DynamicArray<VisibleLightEntityInfo>& visibleLights)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(CanRenderShadows());
	SPT_CHECK(m_shadowMaps.size() % 6 == 0);

	m_lightsWithUpdatedShadowMaps.clear();

	const SizeType availableShadowMapsNum = m_shadowMaps.size() / 6;
	const SizeType shadowMapsInUse = std::min(availableShadowMapsNum, visibleLights.size());
		
	const auto compareOp = [](const VisibleLightEntityInfo& lhs, const VisibleLightEntityInfo& rhs) { return lhs.areaOnScreen > rhs.areaOnScreen; };

	// determine most visible lights. Those lights should have shadow maps of best quality
	std::nth_element(std::begin(visibleLights), std::begin(visibleLights) + shadowMapsInUse, std::end(visibleLights), compareOp);
	std::sort(std::begin(visibleLights), std::begin(visibleLights) + shadowMapsInUse, compareOp);

	lib::DynamicArray<std::pair<RenderSceneEntity, EShadowMapQuality>> shadowMapsToUpgrade;

	struct ShadowMapReleaseInfo
	{
		RenderSceneEntity entity;
		EShadowMapQuality desiredQualityToAcquire;
	};
	
	// current quality -> release info
	lib::HashMap<EShadowMapQuality, lib::DynamicArray<ShadowMapReleaseInfo>> shadowMapsToRelease;

	const lib::HashMap<RenderSceneEntity, EShadowMapQuality> pointLightsWithAssignedShadowMapsPrevFrame = std::move(m_pointLightsWithAssignedShadowMaps);

	lib::HashSet<RenderSceneEntity> lightsVisibleCurrentFrame;
	lightsVisibleCurrentFrame.reserve(shadowMapsInUse);

	// Iterate over lights that currently use shadow maps to determine if their quality should be changed

	for (SizeType lightIdx = 0; lightIdx < shadowMapsInUse; ++lightIdx)
	{
		const RenderSceneEntity lightEntity = visibleLights[lightIdx].entity;

		lightsVisibleCurrentFrame.emplace(lightEntity);

		const auto foundCurrentQuality = pointLightsWithAssignedShadowMapsPrevFrame.find(lightEntity);
		const EShadowMapQuality currentQuality = foundCurrentQuality != std::cend(pointLightsWithAssignedShadowMapsPrevFrame) ? foundCurrentQuality->second : EShadowMapQuality::None;
		const EShadowMapQuality desiredQuality = GetShadowMapQuality(lightIdx);

		if (static_cast<Uint32>(currentQuality) < static_cast<Uint32>(desiredQuality))
		{
			// quality is lower than it should. Check if we can upgrade one more shadow map this frame
			if (shadowMapsToUpgrade.size() < static_cast<SizeType>(params::maxShadowMapsUpgradedPerFrame.GetValue()))
			{
				if (currentQuality != EShadowMapQuality::None)
				{
					ReleaseShadowMap(currentQuality, ResetPointLightShadowMap(lightEntity));
				}

				shadowMapsToUpgrade.emplace_back(std::make_pair(lightEntity, desiredQuality));
			}
			else if (currentQuality != EShadowMapQuality::None)
			{
				// this shadow map should be upgraded but we're out of update budget
				// Because of that we're just marking this shadow map as "shadowMapsToRelease"
				// This will allow using this shadow map for other lights if necessary, and if not, it will stay with same quality
				shadowMapsToRelease[currentQuality].emplace_back(ShadowMapReleaseInfo{ lightEntity, desiredQuality });
			}
		}
		else if (static_cast<Uint32>(currentQuality) > static_cast<Uint32>(desiredQuality))
		{
			// This shadow map should be downgraded
			shadowMapsToRelease[currentQuality].emplace_back(ShadowMapReleaseInfo{ lightEntity, desiredQuality });
		}
		else
		{
			// This light use shadow map of proper quality
			m_pointLightsWithAssignedShadowMaps[lightEntity] = desiredQuality;
		}
	}

	SPT_MAYBE_UNUSED
	const auto smToUpgradeCopy = shadowMapsToUpgrade;

	SPT_MAYBE_UNUSED
	const auto smToDowngradeCopy = shadowMapsToRelease;

	lib::DynamicArray<RenderSceneEntity> releasedShadowMaps;

	// release all shadow maps that are not visible
	for (const auto [entity, quality] : pointLightsWithAssignedShadowMapsPrevFrame)
	{
		if (!lightsVisibleCurrentFrame.contains(entity))
		{
			ReleaseShadowMap(quality, ResetPointLightShadowMap(entity));
			releasedShadowMaps.emplace_back(entity);
		}
	}

	// later we treat this array as stack. Reverse it to start upgrading from lowest quality
	std::reverse(std::begin(shadowMapsToUpgrade), std::end(shadowMapsToUpgrade));

	while (!shadowMapsToUpgrade.empty())
	{
		const auto [acquireLightEntity, acquireDesiredQuality] = shadowMapsToUpgrade.back();
		shadowMapsToUpgrade.pop_back();

		Uint32 shadowMapIdx = AcquireAvaialableShadowMap(acquireDesiredQuality);
		if (shadowMapIdx == idxNone<Uint32>)
		{
			// We don't have any available shadow map - try downgrade another light to use its shadow map
			lib::DynamicArray<ShadowMapReleaseInfo>& shadowMapsOfQualityToRelease = shadowMapsToRelease[acquireDesiredQuality];
			const ShadowMapReleaseInfo releaseInfo = shadowMapsOfQualityToRelease.back();
			shadowMapsOfQualityToRelease.pop_back();

			shadowMapIdx = ResetPointLightShadowMap(releaseInfo.entity);

			if (releaseInfo.desiredQualityToAcquire != EShadowMapQuality::None)
			{
				// If downgraded light needs shadow map, we need to acquire it this frame to disable popping
				shadowMapsToUpgrade.emplace_back(std::make_pair(releaseInfo.entity, releaseInfo.desiredQualityToAcquire));
			}
		}
	
		SetPointLightShadowMapsBeginIdx(acquireLightEntity, shadowMapIdx);
		m_pointLightsWithAssignedShadowMaps[acquireLightEntity] = acquireDesiredQuality;
	}

	// Some lights had shadow maps that should be downgraded but there were no other lights to acquire their shadow maps
	// In this case we just leave shadow maps that they currently use, event though quality is too high
	// As these lights were not assigned to any shadow map this frame, we need to do this here, when we're sure that this light will still use same shadow map
	for (const auto [releasedQuality, releaseInfos] : shadowMapsToRelease)
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
	m_updatePriorities.emplace_back(LightUpdatePriority{ pointLightEntity, 0.f });
}

Uint32 ShadowMapsManagerSystem::ResetPointLightShadowMap(RenderSceneEntity pointLightEntity)
{
	Uint32 shadowMapFirstFaceIdx = idxNone<Uint32>;

	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();
	const PointLightShadowMapComponent* shadowMapComp = registry.try_get<PointLightShadowMapComponent>(pointLightEntity);
	if (shadowMapComp)
	{
		shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
		registry.remove<PointLightShadowMapComponent>(pointLightEntity);

		const auto foundUpdatePriority = std::find_if(std::cbegin(m_updatePriorities), std::cend(m_updatePriorities),
													  [pointLightEntity](const LightUpdatePriority& updatePriority)
													  {
														  return updatePriority.light == pointLightEntity;
													  });

		SPT_CHECK(foundUpdatePriority != std::cend(m_updatePriorities));
		m_updatePriorities.erase(foundUpdatePriority);
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

	SPT_CHECK(m_shadowMaps.empty());

	const rhi::EFragmentFormat shadowMapsFormat = ShadowMapRenderStage::GetDepthFormat();

	const auto CreateShadowMapsHelper = [this, shadowMapsFormat](math::Vector2u resolution, SizeType shadowMapsNum, EShadowMapQuality quality)
	{
		const Uint32 prevShadowMapsNum = static_cast<Uint32>(m_shadowMaps.size());

		const SizeType shadowMapTexturesToCreate = shadowMapsNum * 6;
		m_shadowMaps.reserve(m_shadowMaps.size() + shadowMapTexturesToCreate);
		for (Uint32 i = 0; i < shadowMapTexturesToCreate; ++i)
		{
			rhi::TextureDefinition textureDef;
			textureDef.resolution.head<2>()	= resolution;
			textureDef.resolution.z()		= 1;
			textureDef.usage				= lib::Flags(rhi::ETextureUsage::DepthSetncilRT, rhi::ETextureUsage::SampledTexture);
			textureDef.format				= shadowMapsFormat;
			const lib::SharedRef<rdr::Texture> shadowMap = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Shadow Map"), textureDef, rhi::EMemoryUsage::GPUOnly);
			m_shadowMaps.emplace_back(shadowMap);
		}

		lib::DynamicArray<Uint32>& shadowMapsOfQuality = m_availableShadowMaps[quality];
		for (SizeType idx = prevShadowMapsNum; idx < m_shadowMaps.size(); idx += 6)
		{
			shadowMapsOfQuality.emplace_back(static_cast<Uint32>(idx));
		}
	};

	ShadowMapsSettings shadowMapsSettings;
	const Bool loaded = engn::ConfigUtils::LoadConfigData(shadowMapsSettings, "ShadowMapsSettings.yaml");
	SPT_CHECK(loaded);

	CreateShadowMapsHelper(math::Vector2u::Constant(1024),	shadowMapsSettings.highQualityShadowMaps,	EShadowMapQuality::High);
	CreateShadowMapsHelper(math::Vector2u::Constant(512),	shadowMapsSettings.mediumQualityShadowMaps,	EShadowMapQuality::Medium);
	CreateShadowMapsHelper(math::Vector2u::Constant(256),	shadowMapsSettings.lowQualityShadowMaps,	EShadowMapQuality::Low);

	highQualityShadowMapsMaxIdx		= static_cast<Uint32>(shadowMapsSettings.highQualityShadowMaps);
	mediumQualityShadowMapsMaxIdx	= static_cast<Uint32>(shadowMapsSettings.highQualityShadowMaps + shadowMapsSettings.mediumQualityShadowMaps);

	CreateShadowMapsRenderViews();

	const Uint32 framesInFlightNum = rdr::RendererSettings::Get().framesInFlight;

	m_shadowMapViewsBuffers.reserve(static_cast<SizeType>(framesInFlightNum));

	m_shadowMapViewsData.resize(m_shadowMapsRenderViews.size());

	const Uint64 shadowMapViewsBufferSize = m_shadowMapViewsData.size() * sizeof(ShadowMapViewData);
	const rhi::BufferDefinition shadowMapViewsBufferDef(shadowMapViewsBufferSize, rhi::EBufferUsage::Storage);
	for (Uint32 i = 0; i < framesInFlightNum; ++i)
	{
		m_shadowMapViewsBuffers.emplace_back(rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Shadow Map Views Buffer"), shadowMapViewsBufferDef, rhi::EMemoryUsage::CPUToGPU));
	}
}

void ShadowMapsManagerSystem::CreateShadowMapsRenderViews()
{
	SPT_PROFILER_FUNCTION();
	
	m_shadowMapsRenderViews.reserve(m_shadowMaps.size());

	for (SizeType idx = 0; idx < m_shadowMaps.size(); ++idx)
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
		math::Utils::EulerToQuaternionDegrees(0.f,		-90.f,	0.f),	// +Z
		math::Utils::EulerToQuaternionDegrees(0.f,		90.f,	0.f),	// -Z
	};

	for (SizeType faceIdx = 0; faceIdx < faceRotations.size(); ++faceIdx)
	{
		const SizeType renderViewIdx = static_cast<SizeType>(shadowMapBeginIdx) + faceIdx;

		const lib::UniquePtr<RenderView>& renderView = m_shadowMapsRenderViews[renderViewIdx];
		const RenderSceneEntityHandle renderViewEntity = renderView->GetViewEntity();

		const lib::SharedRef<rdr::Texture>& shadowMapTexture = m_shadowMaps[renderViewIdx];
		const math::Vector2u shadowMapResolution = shadowMapTexture->GetResolution2D();

		ShadowMapViewComponent shadowMapViewComp;
		shadowMapViewComp.shadowMap		= shadowMapTexture;
		shadowMapViewComp.owningLight	= owningLight;
		shadowMapViewComp.faceIdx		= static_cast<Uint32>(faceIdx);
		renderViewEntity.emplace_or_replace<ShadowMapViewComponent>(shadowMapViewComp);

		constexpr Real32 nearPlane = 0.1f;
		renderView->SetShadowPerspectiveProjection(math::Utils::DegreesToRadians(90.f), 1.f, nearPlane, pointLight.radius);
		renderView->SetLocation(pointLight.location);
		renderView->SetRotation(faceRotations[faceIdx]);

		renderView->SetRenderingResolution(shadowMapResolution);

		m_shadowMapViewsData[renderViewIdx].viewProjectionMatrix = renderView->GenerateViewProjectionMatrix();
	}
}

void ShadowMapsManagerSystem::UpdateShadowMaps()
{
	SPT_PROFILER_FUNCTION();

	const Real32 deltaTime = engn::Engine::Get().GetEngineTimer().GetDeltaTime();

	const auto GetPriorityMultiplierForQuality = [](EShadowMapQuality quality)
	{
		switch (quality)
		{
		case rsc::EShadowMapQuality::Low:		return 0.5f;
		case rsc::EShadowMapQuality::Medium:	return 1.f;
		case rsc::EShadowMapQuality::High:		return 2.f;

		default:

			SPT_CHECK_NO_ENTRY();
			return 0.f;
		}
	};

	// Create hash set of lights that are already scheduled for update to be able to filter them faster
	lib::HashSet<RenderSceneEntity> lightsToUpdate;
	lightsToUpdate.reserve(m_lightsWithUpdatedShadowMaps.size());
	std::copy(std::cbegin(m_lightsWithUpdatedShadowMaps), std::cend(m_lightsWithUpdatedShadowMaps), std::inserter(lightsToUpdate, std::begin(lightsToUpdate)));

	// Update priorities
	for (LightUpdatePriority& lightUpdatePriority : m_updatePriorities)
	{
		if (lightsToUpdate.contains(lightUpdatePriority.light))
		{
			lightUpdatePriority.updatePriority = 0.f;
		}
		else
		{
			lightUpdatePriority.updatePriority += deltaTime * GetPriorityMultiplierForQuality(m_pointLightsWithAssignedShadowMaps.at(lightUpdatePriority.light));
		}
	}

	if (m_lightsWithUpdatedShadowMaps.size() < params::maxShadowMapsUpdatedPerFrame)
	{
		std::sort(std::begin(m_updatePriorities), std::end(m_updatePriorities),
				  [](const LightUpdatePriority& lhs, const LightUpdatePriority& rhs)
				  {
					  return lhs.updatePriority > rhs.updatePriority;
				  });

		// Add lights that have greatest update priority
		for (SizeType idx = 0; idx < m_updatePriorities.size() && m_lightsWithUpdatedShadowMaps.size() < params::maxShadowMapsUpdatedPerFrame; ++idx)
		{
			const RenderSceneEntity light = m_updatePriorities[idx].light;
			if (!lightsToUpdate.contains(light))
			{
				m_lightsWithUpdatedShadowMaps.emplace_back(light);
				m_updatePriorities[idx].updatePriority = 0.f;
			}
		}
	}
}

void ShadowMapsManagerSystem::CreateShadowMapsDescriptorSet()
{
	m_shadowMapsDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowMapsDS>(RENDERER_RESOURCE_NAME("ShadowMapsDS"));
	for (const lib::SharedPtr<rdr::Texture>& shadowMap : m_shadowMaps)
	{
		rhi::TextureViewDefinition  shadowMapViewDef;
		shadowMapViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Depth);
		m_shadowMapsDS->u_shadowMaps.BindTexture(shadowMap->CreateView(RENDERER_RESOURCE_NAME("Shadow Map View"), shadowMapViewDef));
	}

	UpdateShadowMapsDSViewsData();
}

void ShadowMapsManagerSystem::UpdateShadowMapsDSViewsData()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<rdr::Buffer>& buffer = m_shadowMapViewsBuffers[0];

	gfx::UploadDataToBuffer(lib::Ref(buffer), 0, reinterpret_cast<const Byte*>(m_shadowMapViewsData.data()), m_shadowMapViewsData.size() * sizeof(ShadowMapViewData));
	m_shadowMapsDS->u_shadowMapViews = buffer->CreateFullView();

	m_shadowMapViewsBuffers.emplace_back(buffer);
	m_shadowMapViewsBuffers.erase(std::cbegin(m_shadowMapViewsBuffers));
}

} // spt::rsc
