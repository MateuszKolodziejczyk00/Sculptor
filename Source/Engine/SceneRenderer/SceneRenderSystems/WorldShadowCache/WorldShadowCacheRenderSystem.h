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

	explicit WorldShadowCacheRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void Initialize(lib::MemoryArena& arena, RenderScene& renderScene);
	void Update(const SceneUpdateContext& context);
	void UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData);
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	// End SceneRenderSystem overrides

private:

	// Shaders sample the front shadow map, while tiles of the next placement are rendered to the back one. Swapped when all tiles are rendered.
	struct CascadeData
	{
		CascadeData() = default;
		~CascadeData();

		CascadeData(const CascadeData&) = delete;
		CascadeData& operator=(const CascadeData&) = delete;

		lib::StaticArray<lib::SharedPtr<rdr::Texture>,     2u> shadowMaps;
		lib::StaticArray<lib::SharedPtr<rdr::TextureView>, 2u> shadowMapsViews;

		lib::StaticArray<RenderView*, constants::maxTilesPerAxis * constants::maxTilesPerAxis> tileViews = {};

		math::Matrix4f frontViewProj   = math::Matrix4f::Identity();
		math::Matrix4f pendingViewProj = math::Matrix4f::Identity();

		Uint32 frontIdx    = 0u;
		Uint32 nextTileIdx = 0u;
	};

	void InitializeCascade(CascadeData& cascade, Uint32 cascadeIdx) const;

	void BeginCycle(const RenderView& mainView, CascadeData& cascade, Uint32 cascadeIdx, Uint32 tilesPerAxis) const;

	RenderView* RenderNextTile(const RenderView& mainView, CascadeData& cascade, Uint32 cascadeIdx, Uint32 tilesPerAxis) const;

	Uint32 GetTilesPerAxis(Uint32 cascadeIdx) const;

	lib::StaticArray<CascadeData, constants::cascadeCount> m_cascades;

	Bool m_timeSlicingEnabled = false;

	lib::Span<RenderView*> m_viewsToUpdate;
};

} // spt::rsc::wsc
