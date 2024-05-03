#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Geometry/GeometryRenderer.h"


namespace spt::rsc
{

class VisibilityBufferRenderStage : public RenderStage<VisibilityBufferRenderStage, ERenderStage::VisibilityBuffer>
{
protected:

	using Super = RenderStage<VisibilityBufferRenderStage, ERenderStage::VisibilityBuffer>;

public:

	static rhi::EFragmentFormat GetDepthFormat();

	VisibilityBufferRenderStage();

	// Begin RenderStageBase interface
	virtual void BeginFrame(const RenderScene& renderScene, const RenderView& renderView) override;
	// End RenderStageBase interface

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareDepthTextures(const RenderView& renderView);

	void ExecuteVisbilityBufferRendering(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

	lib::SharedPtr<rdr::TextureView> m_currentDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_currentHiZTexture;
	lib::SharedPtr<rdr::TextureView> m_historyDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_historyHiZTexture;

	lib::SharedPtr<rdr::TextureView> m_currentDepthHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyDepthHalfRes;
	
	lib::SharedPtr<rdr::TextureView> m_visibilityTexture;

	GeometryRenderer m_geometryVisRenderer;
};

} // spt::rsc
