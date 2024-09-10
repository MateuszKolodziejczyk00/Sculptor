#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

struct DepthPrepassMetaData
{
	Bool allowJittering = true;
};


class DepthPrepassRenderStage : public RenderStage<DepthPrepassRenderStage, ERenderStage::DepthPrepass>
{
protected:

	using Super = RenderStage<DepthPrepassRenderStage, ERenderStage::DepthPrepass>;

public:

	static rhi::EFragmentFormat GetDepthFormat();

	DepthPrepassRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareDepthTextures(const ViewRenderingSpec& viewSpec);

	void ExecuteDepthPrepass(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, rg::RGTextureViewHandle depthTarget, const DepthPrepassMetaData& metaData);

	lib::SharedPtr<rdr::TextureView> m_currentDepth;
	lib::SharedPtr<rdr::TextureView> m_currentDepthHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyDepth;
	lib::SharedPtr<rdr::TextureView> m_historyDepthHalfRes;
};

} // spt::rsc
