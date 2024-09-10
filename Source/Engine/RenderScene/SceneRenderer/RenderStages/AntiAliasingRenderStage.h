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

	virtual void PrepareForRendering(const ViewRenderingSpec& viewSpec) {};
	virtual void Render(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle input) { };


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

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareAntiAliasingTechnique(const ViewRenderingSpec& viewSpec);

	lib::UniquePtr<AntiAliasingTechniqueInterface> m_technique;
};

} // spt::rsc
