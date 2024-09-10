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

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareDepthTextures(const ViewRenderingSpec& viewSpec);

	void CreateGBuffer(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec);

	void ExecuteVisbilityBufferRendering(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

	void RenderGBufferAdditionalTextures(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec);

	lib::SharedPtr<rdr::TextureView> m_currentDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_currentHiZTexture;
	lib::SharedPtr<rdr::TextureView> m_historyDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_historyHiZTexture;

	lib::SharedPtr<rdr::TextureView> m_currentDepthHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyDepthHalfRes;
	
	lib::SharedPtr<rdr::TextureView> m_visibilityTexture;

	lib::SharedPtr<rdr::TextureView> m_externalRoughnessTexture;
	lib::SharedPtr<rdr::TextureView> m_externalHistoryRoughnessTexture;

	TextureWithHistory m_octahedronNormalsTexture;

	TextureWithHistory m_specularColorTexture;

	GeometryRenderer m_geometryVisRenderer;
};

} // spt::rsc
