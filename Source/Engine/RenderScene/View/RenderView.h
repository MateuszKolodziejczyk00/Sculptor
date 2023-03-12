#pragma once

#include "RenderSceneMacros.h"
#include "SceneView.h"
#include "SceneRenderingTypes.h"
#include "RenderSceneRegistry.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc
{

class RenderScene;


BEGIN_SHADER_STRUCT(RenderViewData)
	SHADER_STRUCT_FIELD(math::Vector2u, renderingResolution)
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

	const RenderSceneEntityHandle& GetViewEntity() const;

	const lib::SharedPtr<RenderViewDS>& GetRenderViewDS() const;
	lib::SharedRef<RenderViewDS> GetRenderViewDSRef() const;

	void OnBeginRendering();

private:

	void CreateRenderViewDS();

	ERenderStage m_supportedStages;

	math::Vector2u m_renderingResolution;

	RenderSceneEntityHandle m_viewEntity;

	lib::SharedPtr<RenderViewDS> m_renderViewDS;
};

} // spt::rsc