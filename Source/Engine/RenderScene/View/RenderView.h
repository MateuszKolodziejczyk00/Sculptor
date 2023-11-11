#pragma once

#include "RenderSceneMacros.h"
#include "SceneView.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "RenderSceneRegistry.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ViewRenderSystem.h"
#include "RenderSystemsRegistry.h"


namespace spt::rsc
{

class RenderScene;


namespace EDebugFeature
{
enum Type
{
	None = 0,
	ShowMeshlets,
	ShowIndirectLighting,
	AmbientOcclusion,

	NUM
};
} // EDebugFeature


namespace EAntiAliasingMode
{
enum Type
{
	None = 0,
	TemporalAA,

	NUM
};
} // EAntiAliasingMode


BEGIN_SHADER_STRUCT(RenderViewData)
	SHADER_STRUCT_FIELD(math::Vector2u, renderingResolution)
#if RENDERER_DEBUG
	SHADER_STRUCT_FIELD(Uint32, debugFeatureIndex)
#endif // RENDERER_DEBUG
END_SHADER_STRUCT();


DS_BEGIN(RenderViewDS, rg::RGDescriptorSetState<RenderViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewData>),				u_prevFrameSceneView)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewData>),				u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewCullingData>),			u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<RenderViewData>),				u_viewRenderingParams)
DS_END()


class RENDER_SCENE_API RenderView : public SceneView
{
protected:

	using Super = SceneView;

public:

	explicit RenderView(RenderScene& renderScene);
	~RenderView();

	void SetRenderStages(ERenderStage stages);
	void AddRenderStages(ERenderStage stagesToAdd);
	void RemoveRenderStages(ERenderStage stagesToRemove);

	ERenderStage GetSupportedStages() const;

	void SetRenderingResolution(const math::Vector2u& resolution);
	const math::Vector2u& GetRenderingResolution() const;
	math::Vector3u GetRenderingResolution3D() const;

	RenderScene&                   GetRenderScene() const;
	const RenderSceneEntityHandle& GetViewEntity() const;

	const lib::MTHandle<RenderViewDS>& GetRenderViewDS() const;

	void SetAntiAliasingMode(EAntiAliasingMode::Type mode);
	EAntiAliasingMode::Type GetAntiAliasingMode() const;

#if RENDERER_DEBUG
	void SetDebugFeature(EDebugFeature::Type debugFeature);
	EDebugFeature::Type GetDebugFeature() const;

	Bool IsAnyDebugFeatureEnabled() const;
#endif // RENDERER_DEBUG

	void OnBeginRendering();

	// Render systems

	const lib::DynamicArray<lib::SharedRef<ViewRenderSystem>>& GetRenderSystems() const;

	template<typename TSystemType>
	lib::SharedPtr<TSystemType> FindRenderSystem() const
	{
		return m_renderSystems.FindRenderSystem<TSystemType>();
	}

	template<typename TSystemType, typename... TArgs>
	void AddRenderSystem(TArgs&&... args)
	{
		ViewRenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>(std::forward<TArgs>(args)...);
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}
	}

	template<typename TSystemType>
	void RemoveRenderSystem()
	{
		lib::SharedPtr<ViewRenderSystem> removedSystem = m_renderSystems.RemoveRenderSystem<TSystemType>();
		if (removedSystem)
		{
			DeinitializeRenderSystem(*removedSystem);
		}
	}

	void CollectRenderViews(const RenderScene& renderScene, INOUT RenderViewsCollector& viewsCollector) const;

private:

	void CreateRenderViewDS();

	void InitializeRenderSystem(ViewRenderSystem& renderSystem);
	void DeinitializeRenderSystem(ViewRenderSystem& renderSystem);

	ERenderStage m_supportedStages;

	math::Vector2u m_renderingResolution;

	RenderSceneEntityHandle m_viewEntity;

	RenderScene& m_renderScene;

	lib::MTHandle<RenderViewDS> m_renderViewDS;

	RenderSystemsRegistry<ViewRenderSystem> m_renderSystems;

	// Rendering settings

	EAntiAliasingMode::Type m_aaMode;

#if RENDERER_DEBUG
	EDebugFeature::Type m_debugFeature;
#endif // RENDERER_DEBUG
};

} // spt::rsc