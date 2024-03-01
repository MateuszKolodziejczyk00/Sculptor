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

	// Begin RenderStageBase interface
	virtual void BeginFrame(const RenderScene& renderScene, const RenderView& renderView) override;
	// End RenderStageBase interface

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void PrepareDepthTextures(const RenderView& renderView);

	void ExecuteDepthPrepass(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, rg::RGTextureViewHandle depthTarget, const DepthPrepassMetaData& metaData);

	lib::SharedPtr<rdr::TextureView> m_currentDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_currentDepthNoJitterTexture;
	lib::SharedPtr<rdr::TextureView> m_currentDepthNoJitterTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> m_historyDepthTexture;
	lib::SharedPtr<rdr::TextureView> m_historyDepthNoJitterTexture;
	lib::SharedPtr<rdr::TextureView> m_historyDepthNoJitterTextureHalfRes;
};

} // spt::rsc
