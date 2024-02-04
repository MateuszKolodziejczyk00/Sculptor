#pragma once

#include "EditorSandboxMacros.h"
#include "UILayers/UIView.h"
#include "Renderer/SandboxRenderer.h"


namespace spt::ed
{

class EDITOR_SANDBOX_API SandboxUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit SandboxUIView(const scui::ViewDefinition& definition);

protected:

	//~ Begin  UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) const override;
	virtual void DrawUI() override;
	//~ End  UIView overrides

private:

	void DrawRendererSettings();

	void DrawJobSystemTestsUI();

	ui::TextureID PrepareViewportTexture(math::Vector2u resolution);

	SandboxRenderer m_renderer;

	lib::HashedString m_sceneViewName;
	lib::HashedString m_sanboxUIViewName;

	lib::HashedString m_renderViewSettingsName;
	lib::HashedString m_renderSceneSettingsName;
	lib::HashedString m_profilerPanelName;

	lib::SharedPtr<rdr::TextureView> m_viewportTexture;
};

} // spt::ed