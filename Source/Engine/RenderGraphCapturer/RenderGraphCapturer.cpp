#include "RenderGraphCapturer.h"
#include "DependenciesBuilder.h"
#include "RGResources/RGNode.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphDebugDecorator.h"
#include "Types/Pipeline/Pipeline.h"
#include "FileSystem/File.h"


namespace spt::rg::capture
{

namespace impl
{

class RGCapturerDecorator : public RenderGraphDebugDecorator
{
public:

	explicit RGCapturerDecorator(const RGCaptureSourceInfo& captureSource);

	// Begin RenderGraphDebugDecorator interface
	virtual void PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies) override;
	// End RenderGraphDebugDecorator interface

	const lib::SharedRef<RGCapture>& GetCapture() const;

private:

	lib::SharedRef<RGCapture> m_capture;

	Bool m_isAddingCaptureCopyNode;
};


RGCapturerDecorator::RGCapturerDecorator(const RGCaptureSourceInfo& captureSource)
	: m_isAddingCaptureCopyNode(false)
	, m_capture(lib::MakeShared<RGCapture>())
{
	m_capture->captureSource = captureSource;
	m_capture->name = lib::File::Utils::CreateFileNameFromTime();
}

void RGCapturerDecorator::PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies)
{
	if (m_isAddingCaptureCopyNode)
	{
		return;
	}

	RGNodeCapture& nodeCapture = m_capture->nodes.emplace_back(RGNodeCapture());
	nodeCapture.name = std::format("{}: {}", node.GetID(), node.GetName().AsString());

	if (node.GetType() == ERenderGraphNodeType::Dispatch)
	{
#if DEBUG_RENDER_GRAPH
		const rg::RGNodeDebugMetaData& nodeExecutionMetaData = node.GetDebugMetaData();
		const rg::RGNodeComputeDebugMetaData& computeExecutionMetaData = std::get<rg::RGNodeComputeDebugMetaData>(nodeExecutionMetaData);
		lib::SharedPtr<rdr::Pipeline> pipeline = rdr::ResourcesManager::GetPipelineObject(computeExecutionMetaData.pipelineStateID);

		RGCapturedComputeProperties capturedProperties;
		capturedProperties.pipelineStatistics = pipeline->GetRHI().GetPipelineStatistics();

		nodeCapture.properties = capturedProperties;
#endif // DEBUG_RENDER_GRAPH
	}

	for (const RGTextureAccessDef& textureAccess : dependencies.textureAccesses)
	{
		const Bool isOutputTexture   = textureAccess.access == ERGTextureAccess::StorageWriteTexture
									|| textureAccess.access == ERGTextureAccess::ColorRenderTarget
									|| textureAccess.access == ERGTextureAccess::DepthRenderTarget
									|| textureAccess.access == ERGTextureAccess::TransferDest;

		if (isOutputTexture)
		{
			const RGTextureViewHandle rgTextureView = textureAccess.textureView;
			const RGTextureHandle rgTexture = rgTextureView->GetTexture();

			const Bool canCapture = rgTexture->TryAppendUsage(rhi::ETextureUsage::TransferSource);
			if (canCapture)
			{
				rhi::TextureDefinition captureTextureDef;
				lib::AddFlags(captureTextureDef.usage, lib::Flags(rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::SampledTexture));
				captureTextureDef.resolution	= rgTextureView->GetResolution();
				captureTextureDef.format		= rgTextureView->GetFormat();
				captureTextureDef.mipLevels		= 1u;
				const lib::SharedRef<rdr::Texture> captureTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(rgTexture->GetName()),
																										 captureTextureDef,
																										 rhi::EMemoryUsage::GPUOnly);

				rhi::TextureViewDefinition captureTextureViewDef;
				captureTextureViewDef.subresourceRange.aspect = rgTextureView->GetSubresourceRange().aspect;
				const lib::SharedRef<rdr::TextureView> captureTextureView = captureTexture->CreateView(RENDERER_RESOURCE_NAME(rgTextureView->GetName()),
																									   captureTextureViewDef);

				RGTextureCapture& textureCapture = nodeCapture.outputTextures.emplace_back(RGTextureCapture());
				textureCapture.textureView = captureTextureView.ToSharedPtr();
				textureCapture.passName    = node.GetName().Get();

				const RGTextureViewHandle rgTextureCapture = graphBuilder.AcquireExternalTextureView(captureTextureView.ToSharedPtr());

				{
					lib::ValueGuard recursionGuard(m_isAddingCaptureCopyNode, true);

					graphBuilder.CopyFullTexture(RG_DEBUG_NAME("RG Capture Copy"), rgTextureView, rgTextureCapture);
				}

				graphBuilder.ReleaseTextureWithTransition(rgTextureCapture->GetTexture(), rhi::TextureTransition::ReadOnly);
			}
		}
	}
}

const lib::SharedRef<RGCapture>& RGCapturerDecorator::GetCapture() const
{
	return m_capture;
}

} // impl

RenderGraphCapturer::RenderGraphCapturer(const RGCaptureSourceInfo& captureSource)
	: m_captureSource(captureSource)
{
}

void RenderGraphCapturer::Capture(RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	m_debugDecorator = lib::MakeShared<impl::RGCapturerDecorator>(m_captureSource);
	graphBuilder.AddDebugDecorator(lib::Ref(m_debugDecorator));
}

Bool RenderGraphCapturer::HasValidCapture() const
{
	return !!m_debugDecorator;
}

lib::SharedPtr<RGCapture> RenderGraphCapturer::GetCapture() const
{
	return m_debugDecorator ? m_debugDecorator->GetCapture().ToSharedPtr() : nullptr;
}

} // spt::rg::capture
