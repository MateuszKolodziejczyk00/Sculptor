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

	explicit SandboxUILayer(const scui::LayerDefinition& definition, const SandboxRenderer& renderer);

protected:

	virtual void DrawUI() override;

private:

	const SandboxRenderer* m_renderer;
};

} // spt::ed