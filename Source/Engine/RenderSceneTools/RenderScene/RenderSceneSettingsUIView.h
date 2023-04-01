#pragma once

#include "RenderSceneToolsMacros.h"
#include "UILayers/UIView.h"


namespace spt::rsc
{

class RenderScene;


class RENDER_SCENE_TOOLS_API RenderSceneSettingsUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit RenderSceneSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderScene>& renderScene);

protected:

	// Begin UIView overrides
	virtual void DrawUI() override;
	// End UIView overrides

private:

	void DrawUIForScene(RenderScene& scene);

	lib::WeakPtr<RenderScene> m_renderScene;
};

} // spt::rsc
