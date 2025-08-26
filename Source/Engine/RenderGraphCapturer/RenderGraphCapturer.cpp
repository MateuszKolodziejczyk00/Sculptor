#include "RenderGraphCapturer.h"
#include "DependenciesBuilder.h"
#include "RGResources/RGNode.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphDebugDecorator.h"
#include "Types/Pipeline/Pipeline.h"
#include "FileSystem/File.h"
#include "Types/Buffer.h"


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

	lib::HashMap<RGTexture*, CapturedTexture*> m_capturedTextures;
	lib::HashMap<RGBuffer*, CapturedBuffer*>   m_capturedBuffers;
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

	m_capture->passes.emplace_back(std::make_unique<CapturedPass>());
	CapturedPass& pass = *m_capture->passes.back();
	pass.name = node.GetName().AsString();

	if (node.GetType() == ERenderGraphNodeType::Dispatch)
	{
#if DEBUG_RENDER_GRAPH
		const rg::RGNodeDebugMetaData& nodeExecutionMetaData = node.GetDebugMetaData();
		const rg::RGNodeComputeDebugMetaData& computeExecutionMetaData = std::get<rg::RGNodeComputeDebugMetaData>(nodeExecutionMetaData);
		lib::SharedPtr<rdr::Pipeline> pipeline = rdr::ResourcesManager::GetPipelineObject(computeExecutionMetaData.pipelineStateID);

		RGCapturedComputeProperties capturedProperties;
		capturedProperties.pipelineStatistics = pipeline->GetRHI().GetPipelineStatistics();

		pass.execProps = capturedProperties;
#endif // DEBUG_RENDER_GRAPH
	}

	for (const RGTextureAccessDef& textureAccess : dependencies.textureAccesses)
	{
		const Bool isOutputTexture = textureAccess.access == ERGTextureAccess::StorageWriteTexture
			|| textureAccess.access == ERGTextureAccess::ColorRenderTarget
			|| textureAccess.access == ERGTextureAccess::DepthRenderTarget
			|| textureAccess.access == ERGTextureAccess::TransferDest;

		const RGTextureViewHandle rgTextureView = textureAccess.textureView;
		const RGTextureHandle rgTexture = rgTextureView->GetTexture();

		CapturedTexture*& capturedTexture = m_capturedTextures[rgTexture.Get()];
		if (!capturedTexture)
		{
			capturedTexture = m_capture->textures.emplace_back(std::make_unique<CapturedTexture>()).get();
			capturedTexture->name            = rgTexture->GetName();
			capturedTexture->definition      = rgTexture->GetTextureRHIDefinition();
			capturedTexture->definition.type = rhi::GetSelectedTextureType(capturedTexture->definition);
		}

		const Bool shouldCreateNewVersion = capturedTexture->versions.empty() || isOutputTexture;
		if (shouldCreateNewVersion)
		{
			rhi::TextureDefinition captureTextureDef;
			lib::AddFlags(captureTextureDef.usage, lib::Flags(rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::SampledTexture));
			captureTextureDef.resolution  = rgTexture->GetResolution();
			captureTextureDef.format      = rgTexture->GetFormat();
			captureTextureDef.arrayLayers = rgTexture->GetTextureDefinition().arrayLayers;
			captureTextureDef.mipLevels   = rgTexture->GetMipLevelsNum();
			const lib::SharedRef<rdr::Texture> captureTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(rgTexture->GetName()),
																									 captureTextureDef,
																									 rhi::EMemoryUsage::GPUOnly);

			rhi::TextureViewDefinition captureTextureViewDef;
			captureTextureViewDef.subresourceRange.aspect = rgTextureView->GetSubresourceRange().aspect;
			const lib::SharedRef<rdr::TextureView> captureTextureView = captureTexture->CreateView(RENDERER_RESOURCE_NAME(rgTextureView->GetName()),
																								   captureTextureViewDef);

			const rg::RGTextureViewHandle rgTextureCapture = graphBuilder.AcquireExternalTextureView(captureTextureView.ToSharedPtr());

			{
				lib::ValueGuard recursionGuard(m_isAddingCaptureCopyNode, true);

				for (Uint32 arrayLayerIdx = 0u; arrayLayerIdx < captureTextureDef.arrayLayers; ++arrayLayerIdx)
				{
					for (Uint32 mipLevelIdx = 0u; mipLevelIdx < captureTextureDef.mipLevels; ++mipLevelIdx)
					{
						const RGTextureViewHandle srcMipView = graphBuilder.CreateTextureMipView(rgTexture, mipLevelIdx, arrayLayerIdx);
						const RGTextureViewHandle dstMipView = graphBuilder.CreateTextureMipView(rgTextureCapture, mipLevelIdx, arrayLayerIdx);
						graphBuilder.CopyFullTexture(RG_DEBUG_NAME("RG Capture Mip Copy"), srcMipView, dstMipView);
					}
				}
			}


			CapturedTexture::Version& newVersion = *capturedTexture->versions.emplace_back(std::make_unique<CapturedTexture::Version>());
			newVersion.owningTexture = capturedTexture;
			newVersion.versionIdx    = static_cast<Uint32>(capturedTexture->versions.size()) - 1u;
			newVersion.texture       = captureTexture;
			newVersion.producingPass = &pass; // might be null in case of first version of external texture

			graphBuilder.ReleaseTextureWithTransition(rgTextureCapture->GetTexture(), rhi::TextureTransition::ReadOnly);
		}

		const CapturedTexture::Version& lastVersion = *capturedTexture->versions.back();

		rhi::TextureViewDefinition captureTextureMipViewDef;
		captureTextureMipViewDef.subresourceRange = rgTextureView->GetSubresourceRange();

		CapturedTextureBinding& textureBinding = pass.textures.emplace_back();
		textureBinding.textureVersion = &lastVersion;
		textureBinding.textureView    = lastVersion.texture->CreateView(RENDERER_RESOURCE_NAME(rgTextureView->GetName()), captureTextureMipViewDef);
		textureBinding.baseMipLevel   = rgTextureView->GetBaseMipLevel();
		textureBinding.mipLevelsNum   = rgTextureView->GetMipLevelsNum();
		textureBinding.baseArrayLayer = rgTextureView->GetBaseArrayLayer();
		textureBinding.arrayLayersNum = rgTextureView->GetArrayLayersNum();
		textureBinding.writable       = isOutputTexture;

		const rdr::ResourceDescriptorIdx srvDescriptorIdx = rgTextureView->GetSRVDescriptor();
		const rdr::ResourceDescriptorIdx uavDescriptorIdx = rgTextureView->GetUAVDescriptor();

		if (srvDescriptorIdx != rdr::invalidResourceDescriptorIdx)
		{
			m_capture->descriptorIdxToTexture[srvDescriptorIdx] = capturedTexture;
		}
		if (uavDescriptorIdx != rdr::invalidResourceDescriptorIdx)
		{
			m_capture->descriptorIdxToTexture[uavDescriptorIdx] = capturedTexture;
		}
	}

	for (const RGBufferAccessDef& bufferAccess : dependencies.bufferAccesses)
	{
		const rg::RGBufferViewHandle boundBufferView = bufferAccess.resource;
		const rg::RGBufferHandle boundBuffer         = boundBufferView->GetBuffer();

		CapturedBuffer* capturedBuffer = m_capturedBuffers[boundBufferView->GetBuffer().Get()];
		if (!capturedBuffer)
		{
			capturedBuffer = m_capture->buffers.emplace_back(std::make_unique<CapturedBuffer>()).get();
			capturedBuffer->name = boundBufferView->GetBuffer()->GetName();
		}

		const Bool shouldCreatenewVersion = capturedBuffer->versions.empty() || bufferAccess.access == ERGBufferAccess::Write || bufferAccess.access == ERGBufferAccess::ReadWrite;
		if (shouldCreatenewVersion)
		{
			CapturedBuffer::Version& newVersion = *capturedBuffer->versions.emplace_back(std::make_unique<CapturedBuffer::Version>());
			newVersion.owningBuffer  = capturedBuffer;
			newVersion.versionIdx    = static_cast<Uint32>(capturedBuffer->versions.size()) - 1u;
			newVersion.producingPass = &pass;
			if (boundBuffer && boundBuffer->AllowsHostAccess())
			{
				const lib::SharedPtr<rdr::Buffer>& buffer = boundBufferView->GetBuffer()->GetResource();
				SPT_CHECK(!!buffer);

				const rhi::RHIMappedByteBuffer mappedBuffer(buffer->GetRHI());
				newVersion.bufferData.resize(boundBuffer->GetSize());

				std::memcpy(newVersion.bufferData.data(), mappedBuffer.GetPtr() + boundBufferView->GetOffset(), boundBuffer->GetSize());
			}
		}

		const CapturedBuffer::Version& bufferVersion = *capturedBuffer->versions.back();

		CapturedBufferBinding& bufferBinding = pass.buffers.emplace_back();
		bufferBinding.bufferVersion  = &bufferVersion;
		bufferBinding.offset         = boundBufferView->GetOffset();
		bufferBinding.size           = boundBufferView->GetSize();
#if DEBUG_RENDER_GRAPH
		bufferBinding.structTypeName = bufferAccess.structTypeName;
		bufferBinding.elementsNum    = bufferAccess.elementsNum;
#endif // DEBUG_RENDER_GRAPH
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
