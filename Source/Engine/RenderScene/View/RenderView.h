#pragma once

#include "RenderSceneMacros.h"
#include "SceneView.h"
#include "SceneRenderingTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

class RenderScene;


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

private:

	ERenderStage m_supportedStages;

	math::Vector2u m_renderingResolution;

	RenderSceneEntityHandle m_viewEntity;
};

} // spt::rsc