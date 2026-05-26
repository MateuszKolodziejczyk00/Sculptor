#pragma once

#include "EditorSandboxMacros.h"
#include "ViewportInterface.h"
#include "UILayers/UIView.h"
#include "Renderer/SandboxRenderer.h"


namespace spt::ed
{

class EDITOR_SANDBOX_API SandboxUIView : public scui::UIView, public ViewportInterface
{
protected:

	using Super = scui::UIView;

public:

	explicit SandboxUIView(const scui::ViewDefinition& definition);

	virtual Bool IsFocused() const override;

protected:

	//~ Begin  UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	virtual ImGuiWindowFlags GetWindowFlags() const override;
	//~ End  UIView overrides

private:

	void DrawMenuBar();
	void DrawRendererSettings();
	void FocusWindow(const lib::HashedString& windowName);
	void RecreateRenderViewSettings();
	void RecreateRenderSceneSettings();
	void RecreateSceneRendererStats();
	void RecreateProfilerView();
	void RecreateTerrainEditor();

	ui::TextureID PrepareViewportTexture(math::Vector2u resolution);

	SandboxRenderer m_renderer;

	lib::HashedString m_sceneViewName;
	lib::HashedString m_sanboxUIViewName;

	lib::HashedString m_renderViewSettingsName;
	lib::HashedString m_renderSceneSettingsName;
	lib::HashedString m_sceneRendererStatsName;
	lib::HashedString m_profilerPanelName;
	lib::HashedString m_terrainEditorName;

	lib::HashedString m_requestedWindowFocus;

	lib::SharedPtr<scui::UIView> m_renderViewSettingsView;
	lib::SharedPtr<scui::UIView> m_renderSceneSettingsView;
	lib::SharedPtr<scui::UIView> m_sceneRendererStatsView;
	lib::SharedPtr<scui::UIView> m_profilerPanelView;
	lib::SharedPtr<scui::UIView> m_terrainEditorView;

	lib::SharedPtr<rdr::TextureView> m_viewportTexture;
};

} // spt::ed
