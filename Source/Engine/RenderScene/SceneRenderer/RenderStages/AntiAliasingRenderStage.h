#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class AntiAliasingTechniqueInterface
{
public:

	AntiAliasingTechniqueInterface(EAntiAliasingMode::Type mode)
		: m_mode(mode)
	{ }

	virtual ~AntiAliasingTechniqueInterface() = default;

	virtual void                    BeginFrame(const RenderView& renderView) {};
	virtual rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle input) { return nullptr; };


	EAntiAliasingMode::Type GetMode() const { return m_mode; }

private:

	const EAntiAliasingMode::Type m_mode;
};


class AntiAliasingRenderStage : public RenderStage<AntiAliasingRenderStage, ERenderStage::AntiAliasing>
{
protected:

	using Super = RenderStage<AntiAliasingRenderStage, ERenderStage::AntiAliasing>;

public:

	AntiAliasingRenderStage();

	// Begin RenderStageBase interface
	virtual void BeginFrame(const RenderScene& renderScene, const RenderView& renderView) override;
	// End RenderStageBase interface

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareAntiAliasingTechnique(const RenderView& renderView);

	lib::UniquePtr<AntiAliasingTechniqueInterface> m_technique;
};

} // spt::rsc
