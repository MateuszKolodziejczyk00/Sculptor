#pragma once

#include "EditorSandboxMacros.h"
#include "UILayers/UILayer.h"


namespace spt::ed
{

class SandboxRenderer;


class EDITOR_SANDBOX_API SandboxUILayer : public scui::UILayer
{
protected:

	using Super = scui::UILayer;

public:

	explicit SandboxUILayer(const scui::LayerDefinition& definition, SandboxRenderer& renderer);

protected:

	//~ Begin  UILayer overrides
	virtual void DrawUI() override;
	//~ End  UILayer overrides

private:

	void DrawRendererSettings();

	void DrawJobSystemTestsUI();

	SandboxRenderer* m_renderer;
};

} // spt::ed