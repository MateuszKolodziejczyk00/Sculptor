#pragma once

#include "RenderSceneMacros.h"
#include "SceneView.h"
#include "Blackboard.h"


namespace spt::rsc
{

class RenderScene;
class RenderStageBase;


class RENDER_SCENE_API RenderView : public SceneView
{
protected:

	using Super = SceneView;

public:

	RenderView();
	~RenderView();

	void BeginFrame(math::Vector2u renderingRes);

	void                  SetOutputRes(const math::Vector2u& resolution);
	const math::Vector2u& GetOutputRes() const;
	math::Vector3u        GetOutputRes3D() const;

	lib::Blackboard&       GetBlackboard();
	const lib::Blackboard& GetBlackboard() const;

private:

	math::Vector2u m_outputResolution;

	lib::Blackboard m_blackboard;
};

} // spt::rsc
