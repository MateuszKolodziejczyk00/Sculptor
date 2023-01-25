#pragma once

#include "RenderSystem.h"
#include "Pipelines/PipelineState.h"

namespace spt::rsc
{

class RENDER_SCENE_API StaticMeshesRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshesRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	// Depth Prepass =================================

	Bool BuildDepthPrepassBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void CullDepthPrepassPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void RenderDepthPrepassPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	// Forward Opaque ================================

	Bool BuildForwardOpaueBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void CullForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void RenderForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	// Depth Prepass

	rdr::PipelineStateID m_buildDrawCommandsPipeline;

	rdr::PipelineStateID m_depthPrepassRenderingPipeline;

	// Forward Opaque

	rdr::PipelineStateID m_cullInstancesPipeline;
	rdr::PipelineStateID m_cullSubmeshesPipeline;
	rdr::PipelineStateID m_cullMeshletsPipeline;
	rdr::PipelineStateID m_cullTrianglesPipeline;

	rdr::PipelineStateID m_forwadOpaqueShadingPipeline;
};

} // spt::rsc
