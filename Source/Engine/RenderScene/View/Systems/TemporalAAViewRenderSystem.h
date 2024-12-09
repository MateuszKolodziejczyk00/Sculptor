#pragma once

#include "ViewRenderSystem.h"
#include "View/ViewRenderingSpec.h"


namespace spt::gfx
{
class TemporalAARenderer;
} // spt::gfx


namespace spt::rsc
{

class RENDER_SCENE_API TemporalAAViewRenderSystem : public ViewRenderSystem
{
protected:

	using Super = ViewRenderSystem;

public:

	explicit TemporalAAViewRenderSystem();

	void SetTemporalAARenderer(lib::UniquePtr<gfx::TemporalAARenderer> temporalAARenderer);

	lib::StringView GetRendererName() const;

	// Begin ViewRenderSystem interface
	virtual void PrepareRenderView(RenderView& renderView) override;
	virtual void PreRenderFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End ViewRenderSystem interface

protected:

	// Begin ViewRenderSystem interface
	virtual void OnInitialize(RenderView& inRenderView) override;
	// End ViewRenderSystem interface

private:

	void OnRenderAntiAliasingStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);

	std::optional<lib::UniquePtr<gfx::TemporalAARenderer>> m_requestedTemporalAARenderer;
	mutable lib::Lock m_temporalAARendererLock;

	lib::UniquePtr<gfx::TemporalAARenderer> m_temporalAARenderer;

	Bool m_canRenderAA = false;
};

} // spt::rsc
