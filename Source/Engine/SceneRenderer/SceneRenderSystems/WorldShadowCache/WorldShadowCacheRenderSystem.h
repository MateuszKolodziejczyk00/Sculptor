#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "Allocators/MemoryArena.h"
#include "RenderSceneRegistry.h"
#include "WorldShadowCacheTypes.h"


namespace spt::rsc::wsc
{

class RENDER_SCENE_API WorldShadowCacheRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::WorldShadowCache;

	explicit WorldShadowCacheRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void Initialize(lib::MemoryArena& arena, RenderScene& renderScene);
	void Update(const SceneUpdateContext& context);
	void UpdateGPUSceneData(RenderSceneConstants& sceneData);
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	// End SceneRenderSystem overrides

private:

	struct PerLightData
	{
		PerLightData() = default;
		~PerLightData();

		PerLightData(const PerLightData&) = delete;
		PerLightData& operator=(const PerLightData&) = delete;

		PerLightData(PerLightData&& other)
			: shadowMaps(std::move(other.shadowMaps))
			, shadowMapsViews(std::move(other.shadowMapsViews))
			, shadowCascadeViews(std::move(other.shadowCascadeViews))
		{
			std::fill(other.shadowCascadeViews.begin(), other.shadowCascadeViews.end(), nullptr);
		}

		PerLightData& operator=(PerLightData&& other)
		{
			if (this != &other)
			{
				shadowMaps = std::move(other.shadowMaps);
				shadowMapsViews = std::move(other.shadowMapsViews);
				shadowCascadeViews = std::move(other.shadowCascadeViews);
				std::fill(other.shadowCascadeViews.begin(), other.shadowCascadeViews.end(), nullptr);
			}
			return *this;
		}

		lib::StaticArray<lib::SharedPtr<rdr::Texture>,     constants::cascadeCount> shadowMaps;
		lib::StaticArray<lib::SharedPtr<rdr::TextureView>, constants::cascadeCount> shadowMapsViews;

		lib::StaticArray<RenderView*, constants::cascadeCount> shadowCascadeViews;
	};

	PerLightData CreatePerLightData() const;

	void UpdateCascadeViewsMatrices(const RenderView& mainView, PerLightData& dirLightData, Uint32 cascadeIdx) const;

	PerLightData m_dirLightData;

	lib::Span<RenderView*> m_cascadesToUpdate;
};

} // spt::rsc::wsc
