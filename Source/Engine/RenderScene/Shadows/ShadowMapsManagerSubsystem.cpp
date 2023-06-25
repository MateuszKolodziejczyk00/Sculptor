#include "ShadowMapsManagerSubsystem.h"
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
#include "EngineFrame.h"

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

RendererBoolParameter enableShadows("Enable Local Shadows", { "Lighting", "Shadows", "Local" }, true);
RendererIntParameter maxShadowMapsUpgradedPerFrame("Max ShadowMaps Upgraded Per Frame", { "Lighting", "Shadows", "Local" }, 1, 0, 10);
RendererIntParameter maxShadowMapsUpdatedPerFrame("Max ShadowMaps Updated Per Frame", { "Lighting", "Shadows", "Local" }, 0, 0, 10);

} // params


namespace constants
{

const math::Vector2u highQualitySMResolution	= math::Vector2u::Constant(1024);
const math::Vector2u mediumQualitySMResolution	= math::Vector2u::Constant(512);
const math::Vector2u lowQualitySMResolution		= math::Vector2u::Constant(256);

const Real32 projectionNearPlane = 0.04f;

} // constants


ShadowMapsManagerSubsystem::ShadowMapsManagerSubsystem(RenderScene& owningScene, const lib::SharedPtr<RenderView>& inMainView)
	: Super(owningScene)
	, m_highQualityShadowMapLightEndIdx(0)
	, m_mediumQualityShadowMapsLightEndIdx(0)
	, m_mainView(inMainView)
	, m_shadowMapTechnique(EShadowMappingTechnique::DPCF)
{
	CreateShadowMaps();

	CreateShadowMapsDescriptorSet();
}

void ShadowMapsManagerSubsystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	AssignShadowMaps();

	FindShadowMapsToUpdate();

	RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	lib::DynamicArray<lib::UniquePtr<RenderView>> shadowMapViws;

	const lib::DynamicArray<RenderSceneEntity>& m_pointLightsWithShadowMapsToUpdate = GetPointLightsWithShadowMapsToUpdate();
	for (const RenderSceneEntity lightEntity : m_pointLightsWithShadowMapsToUpdate)
	{
		const PointLightData& pointLightData					= sceneRegistry.get<PointLightData>(lightEntity);
		const PointLightShadowMapComponent& pointLightShadowMap	= sceneRegistry.get<PointLightShadowMapComponent>(lightEntity);

		UpdateShadowMapRenderViews(lightEntity, pointLightData, pointLightShadowMap.shadowMapFirstFaceIdx);
	}

	if (!m_pointLightsWithShadowMapsToUpdate.empty())
	{
		UpdateShadowMapsDSViewsData();
	}
}

void ShadowMapsManagerSubsystem::SetShadowMappingTechnique(EShadowMappingTechnique newTechnique)
{
	if (m_shadowMapTechnique != newTechnique)
	{
		m_shadowMapTechnique = newTechnique;

		RecreateShadowMaps();
	}
}

EShadowMappingTechnique ShadowMapsManagerSubsystem::GetShadowMappingTechnique() const
{
	return m_shadowMapTechnique;
}

void ShadowMapsManagerSubsystem::UpdateVisibleLocalLights(const lib::SharedPtr<rdr::Buffer>& visibleLightsBuffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!visibleLightsBuffer);

	m_visibleLocalLightsSet.clear();
	
	const rhi::RHIBuffer& rhiBuffer = visibleLightsBuffer->GetRHI();
	const rhi::RHIMappedBuffer<RenderSceneEntity> visibleLightsData(rhiBuffer);

	const SizeType maxVisibleLights = visibleLightsData.GetElementsNum();
	m_visibleLocalLightsSet.reserve(maxVisibleLights);

	for (SizeType idx = 0; idx < maxVisibleLights; ++idx)
	{
		const RenderSceneEntity lightEntityID = visibleLightsData[idx];
		if (lightEntityID == idxNone<RenderSceneEntity>)
		{
			break;
		}

		m_visibleLocalLightsSet.emplace(lightEntityID);
	}
}

Bool ShadowMapsManagerSubsystem::IsMainView(const RenderView& renderView) const
{
	return m_mainView.lock().get() == &renderView;
}

Bool ShadowMapsManagerSubsystem::CanRenderShadows() const
{
	return !m_shadowMaps.empty();
}

const lib::SharedPtr<ShadowMapsDS>& ShadowMapsManagerSubsystem::GetShadowMapsDS() const
{
	return m_shadowMapsDS;
}

const lib::DynamicArray<RenderSceneEntity>& ShadowMapsManagerSubsystem::GetPointLightsWithShadowMapsToUpdate() const
{
	return m_lightsWithShadowMapsToUpdate;
}

lib::DynamicArray<RenderView*> ShadowMapsManagerSubsystem::GetShadowMapViewsToUpdate() const
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

lib::DynamicArray<RenderView*> ShadowMapsManagerSubsystem::GetPointLightShadowMapViews(const PointLightShadowMapComponent& pointLightShadowMap) const
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

void ShadowMapsManagerSubsystem::SetPointLightShadowMapsBeginIdx(RenderSceneEntity pointLightEntity, Uint32 shadowMapBeginIdx)
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();
	registry.emplace<PointLightShadowMapComponent>(pointLightEntity, PointLightShadowMapComponent{ shadowMapBeginIdx });
	m_lightsWithShadowMapsToUpdate.emplace_back(pointLightEntity);
	m_updatePriorities.emplace_back(LightUpdatePriority{ pointLightEntity, 0.f });
}

Uint32 ShadowMapsManagerSubsystem::ResetPointLightShadowMap(RenderSceneEntity pointLightEntity)
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

		m_pointLightsWithAssignedShadowMaps.erase(pointLightEntity);
	}
	return shadowMapFirstFaceIdx;
}

EShadowMapQuality ShadowMapsManagerSubsystem::GetShadowMapQuality(SizeType pointLightIdx) const
{
	if (pointLightIdx < m_highQualityShadowMapLightEndIdx)
	{
		return EShadowMapQuality::High;
	}
	else if (pointLightIdx < m_mediumQualityShadowMapsLightEndIdx)
	{
		return EShadowMapQuality::Medium;
	}

	return EShadowMapQuality::Low;
}

EShadowMapQuality ShadowMapsManagerSubsystem::GetShadowMapQuality(RenderSceneEntity light) const
{
	const auto foundPriority = m_pointLightsWithAssignedShadowMaps.find(light);
	return foundPriority != std::cend(m_pointLightsWithAssignedShadowMaps) ? foundPriority->second : EShadowMapQuality::None;
}

void ShadowMapsManagerSubsystem::ReleaseShadowMap(EShadowMapQuality quality, Uint32 shadowMapIdx)
{
	m_availableShadowMaps[quality].emplace_back(shadowMapIdx);
}

Uint32 ShadowMapsManagerSubsystem::AcquireAvaialableShadowMap(EShadowMapQuality quality)
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

void ShadowMapsManagerSubsystem::CreateShadowMaps()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_shadowMaps.empty());

	const rhi::EFragmentFormat shadowMapsFormat = GetShadowMapFormat();

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
			textureDef.usage				= GetShadowMapUsage();
			textureDef.format				= shadowMapsFormat;
			textureDef.mipLevels			= GetShadowMapMipsNum();
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

	CreateShadowMapsHelper(constants::highQualitySMResolution,		shadowMapsSettings.highQualityShadowMaps,	EShadowMapQuality::High);
	CreateShadowMapsHelper(constants::mediumQualitySMResolution,	shadowMapsSettings.mediumQualityShadowMaps,	EShadowMapQuality::Medium);
	CreateShadowMapsHelper(constants::lowQualitySMResolution,		shadowMapsSettings.lowQualityShadowMaps,	EShadowMapQuality::Low);

	m_highQualityShadowMapLightEndIdx		= static_cast<Uint32>(shadowMapsSettings.highQualityShadowMaps);
	m_mediumQualityShadowMapsLightEndIdx	= static_cast<Uint32>(shadowMapsSettings.highQualityShadowMaps + shadowMapsSettings.mediumQualityShadowMaps);

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

void ShadowMapsManagerSubsystem::CreateShadowMapsRenderViews()
{
	SPT_PROFILER_FUNCTION();
	
	m_shadowMapsRenderViews.reserve(m_shadowMaps.size());

	for (SizeType idx = 0; idx < m_shadowMaps.size(); ++idx)
	{
		lib::UniquePtr<RenderView>& renderView = m_shadowMapsRenderViews.emplace_back();
		renderView = std::make_unique<RenderView>(GetOwningScene());
		renderView->SetRenderStages(ERenderStage::ShadowMapRendererStages);
	}
}

void ShadowMapsManagerSubsystem::AssignShadowMaps()
{
	SPT_PROFILER_FUNCTION();

	if (!CanRenderShadows())
	{
		return;
	}

	SPT_CHECK(m_shadowMaps.size() % 6 == 0);

	m_lightsWithShadowMapsToUpdate.clear();

	if (!params::enableShadows)
	{
		ReleaseAllShadowMaps();
		return;
	}

	const SizeType availableShadowMapsNum = m_shadowMaps.size() / 6;

	const RenderSceneRegistry& registry = GetOwningScene().GetRegistry();
	const auto pointLightsView = registry.view<PointLightData>();

	const SizeType shadowMapsInUse = std::min(availableShadowMapsNum, pointLightsView.size());
		
	lib::DynamicArray<std::pair<RenderSceneEntity, Real32>> lightPriorities;
	lightPriorities.reserve(pointLightsView.size());

	const lib::SharedPtr<RenderView> sharedView = m_mainView.lock();
	SPT_CHECK(!!sharedView);

	RenderView& view = *sharedView;

	for (const auto& [lightEntity, pointLightData] : pointLightsView.each())
	{
		lightPriorities.emplace_back(std::make_pair(lightEntity, ComputeLocalLightShadowMapPriority(view, lightEntity)));
	}

	const auto compareOp = [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; };

	std::nth_element(std::begin(lightPriorities), std::begin(lightPriorities) + shadowMapsInUse, std::end(lightPriorities), compareOp);
	std::sort(std::begin(lightPriorities), std::begin(lightPriorities) + shadowMapsInUse, compareOp);

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
		const RenderSceneEntity lightEntity = lightPriorities[lightIdx].first;

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

void ShadowMapsManagerSubsystem::UpdateShadowMapRenderViews(RenderSceneEntity owningLight, const PointLightData& pointLight, Uint32 shadowMapBeginIdx)
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

		renderView->SetShadowPerspectiveProjection(math::Utils::DegreesToRadians(90.f), 1.f, constants::projectionNearPlane, pointLight.radius);
		renderView->SetLocation(pointLight.location);
		renderView->SetRotation(faceRotations[faceIdx]);

		renderView->SetRenderingResolution(shadowMapResolution);

		m_shadowMapViewsData[renderViewIdx].viewProjectionMatrix = renderView->GenerateViewProjectionMatrix();
	}
}

void ShadowMapsManagerSubsystem::FindShadowMapsToUpdate()
{
	SPT_PROFILER_FUNCTION();

	const Real32 deltaTime = engn::GetRenderingFrame().GetDeltaTime();

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
	lightsToUpdate.reserve(m_lightsWithShadowMapsToUpdate.size());
	std::copy(std::cbegin(m_lightsWithShadowMapsToUpdate), std::cend(m_lightsWithShadowMapsToUpdate), std::inserter(lightsToUpdate, std::begin(lightsToUpdate)));

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

	if (m_lightsWithShadowMapsToUpdate.size() < params::maxShadowMapsUpdatedPerFrame)
	{
		std::sort(std::begin(m_updatePriorities), std::end(m_updatePriorities),
				  [](const LightUpdatePriority& lhs, const LightUpdatePriority& rhs)
				  {
					  return lhs.updatePriority > rhs.updatePriority;
				  });

		// Add lights that have greatest update priority
		for (SizeType idx = 0; idx < m_updatePriorities.size() && m_lightsWithShadowMapsToUpdate.size() < params::maxShadowMapsUpdatedPerFrame; ++idx)
		{
			const RenderSceneEntity light = m_updatePriorities[idx].light;
			if (!lightsToUpdate.contains(light))
			{
				m_lightsWithShadowMapsToUpdate.emplace_back(light);
				m_updatePriorities[idx].updatePriority = 0.f;
			}
		}
	}
}

void ShadowMapsManagerSubsystem::ReleaseAllShadowMaps()
{
	for (const auto [entity, quality] : m_pointLightsWithAssignedShadowMaps)
	{
		ReleaseShadowMap(quality, ResetPointLightShadowMap(entity));
	}
	m_pointLightsWithAssignedShadowMaps.clear();
}

void ShadowMapsManagerSubsystem::CreateShadowMapsDescriptorSet()
{
	m_shadowMapsDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowMapsDS>(RENDERER_RESOURCE_NAME("ShadowMapsDS"));
	for (const lib::SharedPtr<rdr::Texture>& shadowMap : m_shadowMaps)
	{
		rhi::TextureViewDefinition  shadowMapViewDef;
		shadowMapViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::GetFullAspectForFormat(shadowMap->GetDefinition().format));
		m_shadowMapsDS->u_shadowMaps.BindTexture(shadowMap->CreateView(RENDERER_RESOURCE_NAME("Shadow Map View"), shadowMapViewDef));
	}

	UpdateShadowMapsDSViewsData();

	ShadowsSettings shadowsSettings;
	shadowsSettings.highQualitySMEndIdx			= m_highQualityShadowMapLightEndIdx * 6;
	shadowsSettings.mediumQualitySMEndIdx		= m_mediumQualityShadowMapsLightEndIdx * 6;
	shadowsSettings.highQualitySMPixelSize		= constants::highQualitySMResolution.cast<Real32>().cwiseInverse();
	shadowsSettings.mediumQualitySMPixelSize	= constants::mediumQualitySMResolution.cast<Real32>().cwiseInverse();
	shadowsSettings.lowQualitySMPixelSize		= constants::lowQualitySMResolution.cast<Real32>().cwiseInverse();
	shadowsSettings.shadowViewsNearPlane		= constants::projectionNearPlane;
	shadowsSettings.shadowMappingTechnique		= static_cast<Uint32>(GetShadowMappingTechnique());
	m_shadowMapsDS->u_shadowsSettings = shadowsSettings;
}

void ShadowMapsManagerSubsystem::UpdateShadowMapsDSViewsData()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<rdr::Buffer>& buffer = m_shadowMapViewsBuffers[0];

	gfx::UploadDataToBuffer(lib::Ref(buffer), 0, reinterpret_cast<const Byte*>(m_shadowMapViewsData.data()), m_shadowMapViewsData.size() * sizeof(ShadowMapViewData));
	m_shadowMapsDS->u_shadowMapViews = buffer->CreateFullView();

	m_shadowMapViewsBuffers.emplace_back(buffer);
	m_shadowMapViewsBuffers.erase(std::cbegin(m_shadowMapViewsBuffers));
}

Bool ShadowMapsManagerSubsystem::IsLocalLightVisible(RenderSceneEntity light) const
{
	return m_visibleLocalLightsSet.contains(light);
}

Real32 ShadowMapsManagerSubsystem::ComputeLocalLightShadowMapPriority(const SceneView& view, RenderSceneEntity light) const
{
	const auto getLightCurrentQualityPriority = [](EShadowMapQuality currentQuality)
	{
		switch (currentQuality)
		{
		case spt::rsc::EShadowMapQuality::None:		return 0.f;
		case spt::rsc::EShadowMapQuality::Low:		return 0.25f;
		case spt::rsc::EShadowMapQuality::Medium:	return 0.5f;
		case spt::rsc::EShadowMapQuality::High:		return 1.f;
		}

		SPT_CHECK_NO_ENTRY();
		return 0.f;
	};

	const math::Vector3f viewLocation = view.GetLocation();
	const math::Vector3f viewForward = view.GetForwardVector();

	const PointLightData& pointLightData = GetOwningScene().GetRegistry().get<PointLightData>(light);

	const Real32 maxDistanceToLight	= 15.f;
	const Real32 maxRadius			= 5.f;
	const Real32 maxZDifference		= 7.f;
	const Real32 maxIntensity		= 100.f;

	const Real32 distanceToLight				= (pointLightData.location - (viewLocation + viewForward * 3.f)).norm();
	const math::Vector3f viewToLightDirection	= (pointLightData.location - viewLocation).normalized();
	const Real32 viewAndLightDot				= distanceToLight >= maxRadius ? viewForward.dot(viewToLightDirection) : 1.f;

	const Real32 zDifference = (pointLightData.location.z() - viewLocation.z());

	const Real32 currentQualityMutliplier	= 0.5f;
	const Real32 dotMutliplier				= 4.0f;
	const Real32 distanceMutliplier			= 1.7f;
	const Real32 radiusMutliplier			= 0.6f;
	const Real32 zDifferenceMutliplier		= 0.7f;
	const Real32 intensityMultiplier		= 0.7f;
	const Real32 inRadiusPriority			= 10.f;
	const Real32 visibilityPriority			= 20.f;

	const EShadowMapQuality currentQuality = GetShadowMapQuality(light);

	Real32 priority = 0.f;

	if (distanceToLight < pointLightData.radius)
	{
		priority += inRadiusPriority;
	}

	priority += (1.f - std::clamp(distanceToLight / maxDistanceToLight, 0.f, 1.f)) * distanceMutliplier;
	priority += (viewAndLightDot * 0.5f + 0.5f) * dotMutliplier;
	priority += (1.f - std::clamp(zDifference / maxZDifference, 0.f, 1.f)) * zDifferenceMutliplier;
	priority += getLightCurrentQualityPriority(currentQuality) * currentQualityMutliplier;
	priority += std::clamp(pointLightData.radius / maxRadius, 0.f, 1.f) * radiusMutliplier;
	priority += std::clamp(pointLightData.intensity / maxIntensity, 0.f, 1.f) * intensityMultiplier;
	priority += IsLocalLightVisible(light) ? visibilityPriority : 0.f;

	return priority;
}

const rhi::EFragmentFormat ShadowMapsManagerSubsystem::GetShadowMapFormat() const
{
	switch (GetShadowMappingTechnique())
	{
	case EShadowMappingTechnique::DPCF:
		return rhi::EFragmentFormat::D16_UN_Float;

	case EShadowMappingTechnique::MSM:
		return rhi::EFragmentFormat::RGBA16_UN_Float;

	default:
		SPT_CHECK_NO_ENTRY();
		return rhi::EFragmentFormat::None;
	}
}

const rhi::ETextureUsage ShadowMapsManagerSubsystem::GetShadowMapUsage() const
{
	switch (GetShadowMappingTechnique())
	{
	case EShadowMappingTechnique::DPCF:
		return lib::Flags(rhi::ETextureUsage::DepthSetncilRT, rhi::ETextureUsage::SampledTexture);

	case EShadowMappingTechnique::MSM:
		return lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

	default:
		SPT_CHECK_NO_ENTRY();
		return rhi::ETextureUsage::None;
	}
}

const Uint32 ShadowMapsManagerSubsystem::GetShadowMapMipsNum() const
{
	switch (GetShadowMappingTechnique())
	{
	case EShadowMappingTechnique::DPCF:
		return 1;

	case EShadowMappingTechnique::MSM:
		return 5;

	default:
		SPT_CHECK_NO_ENTRY();
		return idxNone<Uint32>;
	}
}

void ShadowMapsManagerSubsystem::RecreateShadowMaps()
{
	ReleaseAllShadowMaps();

	m_shadowMaps.clear();
	m_shadowMapsRenderViews.clear();
	m_availableShadowMaps.clear();
	m_shadowMapViewsData.clear();
	m_shadowMapViewsBuffers.clear();

	if (GetShadowMappingTechnique() != EShadowMappingTechnique::None)
	{
		CreateShadowMaps();
		CreateShadowMapsDescriptorSet();
	}
}

} // spt::rsc
