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
class RenderStageBase;


class RenderStagesRegistry
{
public:

	RenderStagesRegistry();

	void OnRenderStagesAdded(ERenderStage newStages);
	void OnRenderStagesRemoved(ERenderStage removedStages);

	RenderStageBase* GetRenderStage(ERenderStage stage) const;

	template<typename TCallable>
	void ForEachRenderStage(TCallable&& callable) const
	{
		for (size_t stageIndex = 0; stageIndex < s_maxStages; ++stageIndex)
		{
			if (m_renderStages[stageIndex])
			{
				callable(*m_renderStages[stageIndex]);
			}
		}
	}

private:

	static constexpr size_t s_maxStages = sizeof(ERenderStage) * 8;

	lib::StaticArray<lib::UniquePtr<RenderStageBase>, s_maxStages> m_renderStages;
};


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
	SHADER_STRUCT_FIELD(Real32, preExposure)
#if RENDERER_DEBUG
	SHADER_STRUCT_FIELD(Uint32, debugFeatureIndex)
#endif // RENDERER_DEBUG
END_SHADER_STRUCT();


DS_BEGIN(RenderViewDS, rg::RGDescriptorSetState<RenderViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),        u_prevFrameSceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),        u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>), u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RenderViewData>),       u_viewRenderingParams)
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

	template<typename TStageType>
	TStageType* GetRenderStage() const
	{
		return static_cast<TStageType*>(m_renderStages.GetRenderStage(TStageType::GetStageEnum()));
	}

	template<typename TStageType>
	TStageType& GetRenderStageChecked() const
	{
		TStageType* renderStage = GetRenderStage<TStageType>();
		SPT_CHECK_MSG(!!renderStage, "Render stage not found");
		return *renderStage;
	}

	void SetRenderingRes(const math::Vector2u& resolution);
	const math::Vector2u& GetRenderingRes() const;
	math::Vector3u GetRenderingRes3D() const;

	const math::Vector2u GetRenderingHalfRes() const;

	RenderScene&                   GetRenderScene() const;
	const RenderSceneEntityHandle& GetViewEntity() const;

	const lib::MTHandle<RenderViewDS>& GetRenderViewDS() const;

	void SetAntiAliasingMode(EAntiAliasingMode::Type mode);
	EAntiAliasingMode::Type GetAntiAliasingMode() const;

	void SetPreExposure(Real32 preExposure);
	Real32 GetPreExposure() const;

	math::Vector2f GetCurrentJitter() const;

#if RENDERER_DEBUG
	void SetDebugFeature(EDebugFeature::Type debugFeature);
	EDebugFeature::Type GetDebugFeature() const;

	Bool IsAnyDebugFeatureEnabled() const;
#endif // RENDERER_DEBUG

	void BeginFrame(const RenderScene& renderScene);
	void EndFrame(const RenderScene& renderScene);

	// Render systems

	const lib::DynamicArray<lib::SharedRef<ViewRenderSystem>>& GetRenderSystems() const;

	template<typename TSystemType>
	lib::SharedPtr<TSystemType> FindRenderSystem() const
	{
		return m_renderSystems.FindRenderSystem<TSystemType>();
	}
	
	template<typename TSystemType>
	TSystemType& GetRenderSystem() const
	{
		const lib::SharedPtr<TSystemType> foundSystem = FindRenderSystem<TSystemType>();
		SPT_CHECK_MSG(!!foundSystem, "Render system not found");
		return *foundSystem;
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

	void OnBeginRendering();

	void InitializeRenderSystem(ViewRenderSystem& renderSystem);
	void DeinitializeRenderSystem(ViewRenderSystem& renderSystem);

	ERenderStage m_supportedStages;

	math::Vector2u m_renderingResolution;

	RenderSceneEntityHandle m_viewEntity;

	RenderScene& m_renderScene;

	lib::MTHandle<RenderViewDS> m_renderViewDS;

	RenderSystemsRegistry<ViewRenderSystem> m_renderSystems;

	RenderStagesRegistry m_renderStages;

	// Rendering settings

	EAntiAliasingMode::Type m_aaMode;

	Real32 m_preExposure;

#if RENDERER_DEBUG
	EDebugFeature::Type m_debugFeature;
#endif // RENDERER_DEBUG
};

} // spt::rsc