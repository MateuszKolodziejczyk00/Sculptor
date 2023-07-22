#pragma once

#include "RenderSystem.h"


namespace spt::rsc
{

class RENDER_SCENE_API AtmosphereRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:

	AtmosphereRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) override;
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	void ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	lib::SharedPtr<rdr::TextureView> m_skyIlluminanceTextureView;
};

} // spt::rsc
