#pragma once

#include "EditorSandboxMacros.h"
#include "UILayers/UIView.h"


namespace spt::ed
{

class SandboxRenderer;


class EDITOR_SANDBOX_API SandboxUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit SandboxUIView(const scui::ViewDefinition& definition, SandboxRenderer& renderer);

protected:

	//~ Begin  UIView overrides
	virtual void DrawUI() override;
	//~ End  UIView overrides

private:

	void DrawRendererSettings();

	void DrawJobSystemTestsUI();

	void DrawDebugSceneRenderer();

	SandboxRenderer* m_renderer;
};

} // spt::ed