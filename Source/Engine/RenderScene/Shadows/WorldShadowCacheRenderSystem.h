#pragma once

#include "SceneRenderSystem.h"
#include "Allocators/MemoryArena.h"
#include "WorldShadowCacheTypes.h"


namespace spt::rsc::wsc
{

class RENDER_SCENE_API WorldShadowCacheRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	explicit WorldShadowCacheRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	virtual void Update(const SceneUpdateContext& context) override;
	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) override;
	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	// End SceneRenderSystem overrides

protected:

	// Begin SceneRenderSystem overrides
	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);
	// End SceneRenderSystem overrides

private:

	struct PerLightData
	{
		RenderSceneEntity dirLightEntity;

		lib::StaticArray<lib::SharedPtr<rdr::Texture>,     constants::cascadeCount> shadowMaps;
		lib::StaticArray<lib::SharedPtr<rdr::TextureView>, constants::cascadeCount> shadowMapsViews;

		lib::StaticArray<lib::SharedPtr<RenderView>, constants::cascadeCount> shadowCascadeViews;
	};

	void OnDirectionalLightAdded(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity);

	void UpdateCascadeViewsMatrices(const RenderView& mainView, PerLightData& dirLightData, Uint32 cascadeIdx) const;

	lib::DynamicArray<PerLightData> m_perLightData;

	lib::Span<RenderView*> m_cascadesToUpdate;
};

} // spt::rsc::wsc
