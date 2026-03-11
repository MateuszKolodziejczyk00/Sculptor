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
	void Initialize(RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);
	void Update(const SceneUpdateContext& context);
	void UpdateGPUSceneData(RenderSceneConstants& sceneData);
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	// End SceneRenderSystem overrides

private:

	struct PerLightData
	{
		PerLightData() = default;
		~PerLightData();

		RenderSceneEntity dirLightEntity;

		lib::StaticArray<lib::SharedPtr<rdr::Texture>,     constants::cascadeCount> shadowMaps;
		lib::StaticArray<lib::SharedPtr<rdr::TextureView>, constants::cascadeCount> shadowMapsViews;

		lib::StaticArray<RenderView*, constants::cascadeCount> shadowCascadeViews;
	};

	void OnDirectionalLightAdded(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity);

	void UpdateCascadeViewsMatrices(const RenderView& mainView, PerLightData& dirLightData, Uint32 cascadeIdx) const;

	lib::DynamicArray<PerLightData> m_perLightData;

	lib::Span<RenderView*> m_cascadesToUpdate;
};

} // spt::rsc::wsc
