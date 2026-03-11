#pragma once

#include "RenderSceneToolsMacros.h"
#include "UILayers/UIView.h"


namespace spt::rsc
{

class RenderView;


class RENDER_SCENE_TOOLS_API RenderViewSettingsUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit RenderViewSettingsUIView(const scui::ViewDefinition& definition, RenderView& renderView);

protected:

	// Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	// End UIView overrides

private:

	void DrawUIForView(RenderView& view);

	RenderView& m_renderView;

	lib::HashedString m_renderViewSettingsName;
};

} // spt::rsc
