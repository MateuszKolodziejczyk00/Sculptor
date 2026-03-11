#pragma once

#include "ViewRenderSystems/ViewRenderSystem.h"
#include "Utils/ViewRenderingSpec.h"
#include "Techniques/TemporalAA/StandardTAARenderer.h"
#include "Techniques/TemporalAA/DLSSRenderer.h"
#include "Techniques/TemporalAA/TemporalAccumulationRenderer.h"


namespace spt::gfx
{
class TemporalAARenderer;
} // spt::gfx


namespace spt::rsc
{

class TemporalAAViewRenderSystem : public ViewRenderSystem
{
protected:

	using Super = ViewRenderSystem;

public:

	static constexpr EViewRenderSystem type = EViewRenderSystem::TemporalAASystem;

	explicit TemporalAAViewRenderSystem(RenderView& inRenderView);

	lib::StringView GetRendererName() const;

	// Begin ViewRenderSystem interface
	void PrepareRenderView(ViewRenderingSpec& viewSpec);
	void BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	// End ViewRenderSystem interface

private:

	using TemporalAARendererImpl = std::variant<
		gfx::StandardTAARenderer,
		gfx::DLSSRenderer,
		gfx::TemporalAccumulationRenderer>;

	Bool SupportsUpscaling() const;
	Bool ExecutesUnifiedDenoising() const;

	void ExecuteAA(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	math::Vector2u SelectDesiredRenderingResolution(math::Vector2u outputResolution) const;

	Bool m_canRenderAA = false;

	Uint32 m_jitterFrameIdx = 0u;

	std::optional<TemporalAARendererImpl> m_temporalAARendererImpl;
};

} // spt::rsc
