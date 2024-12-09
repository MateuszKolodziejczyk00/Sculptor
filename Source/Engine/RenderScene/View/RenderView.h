#pragma once

#include "RenderSceneMacros.h"
#include "SceneView.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "RenderSceneRegistry.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
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

	void OnRenderStagesAdded(RenderView& renderView, ERenderStage newStages);
	void OnRenderStagesRemoved(RenderView& renderView, ERenderStage removedStages);

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


BEGIN_SHADER_STRUCT(ViewExposureData)
	SHADER_STRUCT_FIELD(Real32, exposureLastFrame)
	SHADER_STRUCT_FIELD(Real32, rcpExposureLastFrame)
	SHADER_STRUCT_FIELD(Real32, exposure)
	SHADER_STRUCT_FIELD(Real32, rcpExposure)
	SHADER_STRUCT_FIELD(Real32, EV100)
	SHADER_STRUCT_FIELD(Real32, averageLogLuminance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RenderViewData)
	SHADER_STRUCT_FIELD(math::Vector2u, renderingResolution)
END_SHADER_STRUCT();


DS_BEGIN(RenderViewDS, rg::RGDescriptorSetState<RenderViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<SceneViewData>),        u_prevFrameSceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<SceneViewData>),        u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<SceneViewCullingData>), u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<RenderViewData>),       u_viewRenderingParams)
	DS_BINDING(BINDING_TYPE(gfx::OptionalConstantBufferRefBinding<ViewExposureData>),      u_viewExposure)
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

	Uint32 GetRenderedFrameIdx() const;

	const math::Vector2u GetRenderingHalfRes() const;

	RenderScene&                   GetRenderScene() const;
	const RenderSceneEntityHandle& GetViewEntity() const;

	const lib::MTHandle<RenderViewDS>& GetRenderViewDS() const;

	void            SetExposureDataBuffer(lib::SharedPtr<rdr::Buffer> buffer);
	void            ResetExposureDataBuffer();
	rdr::BufferView GetExposureDataBuffer() const;

	void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
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
	TSystemType* AddRenderSystem(TArgs&&... args)
	{
		ViewRenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>(std::forward<TArgs>(args)...);
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}

		return static_cast<TSystemType*>(addedSystem);
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

	void UpdateRenderViewDS();

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

	Uint32 m_renderedFrameIdx;
};

} // spt::rsc