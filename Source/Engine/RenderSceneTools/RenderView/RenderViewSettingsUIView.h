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

	explicit RenderViewSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderView>& renderView);

protected:

	// Begin UIView overrides
	virtual void DrawUI() override;
	// End UIView overrides

private:

	void DrawUIForView(RenderView& view);

	lib::WeakPtr<RenderView> m_renderView;
};

} // spt::rsc