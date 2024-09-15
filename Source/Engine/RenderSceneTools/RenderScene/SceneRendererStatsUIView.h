
#pragma once

#include "RenderSceneToolsMacros.h"
#include "UILayers/UIView.h"


namespace spt::rsc
{

class RenderScene;


class RENDER_SCENE_TOOLS_API SceneRendererStatsUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit SceneRendererStatsUIView(const scui::ViewDefinition& definition);

protected:

	// Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	// End UIView overrides

private:

	lib::HashedString m_statsPanelName;
};

} // spt::rsc
