#include "RGOutputHasher.h"

#if RG_ENABLE_TEST_MODE

#include "RenderGraphBuilder.h"
#include "RenderGraphDebugDecorator.h"
#include "Types/Buffer.h"


namespace spt::rg::test_mode
{

namespace impl
{

class OutputsHasherDecorator : public RenderGraphDebugDecorator
{
public:

	// Begin RenderGraphDebugDecorator interface
	virtual void PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies) override;
	// End RenderGraphDebugDecorator interface

	lib::Span<PassOutputsHashes> ComputeHashes(lib::MemoryArena& outputScratch) const;

private:

	struct OutputDownload
	{
		lib::HashedString           name;
		lib::SharedPtr<rdr::Buffer> data;
	};

	struct PassDownloads
	{
		lib::HashedString                 name;
		lib::DynamicArray<OutputDownload> outputs;
	};

	void DownloadTextureOutputs(RenderGraphBuilder& graphBuilder, const RGDependeciesContainer& dependencies, PassDownloads& pass) const;
	void DownloadBufferOutputs(RenderGraphBuilder& graphBuilder, const RGDependeciesContainer& dependencies, PassDownloads& pass) const;

	lib::DynamicArray<PassDownloads> m_passes;

	Bool m_isAddingDownloads = false;
};


void OutputsHasherDecorator::PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies)
{
	if (m_isAddingDownloads)
	{
		return;
	}

	const lib::ValueGuard downloadsGuard(m_isAddingDownloads, true);

	PassDownloads& pass = m_passes.emplace_back();
	pass.name = node.GetName().ToString();

	DownloadTextureOutputs(graphBuilder, dependencies, pass);
	DownloadBufferOutputs(graphBuilder, dependencies, pass);
}

void OutputsHasherDecorator::DownloadTextureOutputs(RenderGraphBuilder& graphBuilder, const RGDependeciesContainer& dependencies, PassDownloads& pass) const
{
	for (const RGTextureAccessDef& textureAccess : dependencies.textureAccesses)
	{
		if (textureAccess.access == ERGTextureAccess::ShaderRead)
		{
			continue;
		}

		const RGTextureViewHandle textureView = textureAccess.textureView;
		const RGTextureHandle texture         = textureView->GetTexture();

		if (texture->IsNonDeterministic())
		{
			continue;
		}

		const rhi::ETextureAspect aspect = rhi::GetFullAspectForFormat(texture->GetFormat());
		const Bool hasDepthAndStencil    = lib::HasAllFlags(aspect, lib::Flags(rhi::ETextureAspect::Depth, rhi::ETextureAspect::Stencil));

		// Copy to buffer requires transfer source usage, which can't be added to external textures. Copying both depth and stencil aspects at once is not supported
		if ((texture->IsExternal() && !lib::HasAnyFlag(texture->GetTextureRHIDefinition().usage, rhi::ETextureUsage::TransferSource)) || hasDepthAndStencil)
		{
			continue;
		}

		for (Uint32 arrayLayerIdx = textureView->GetBaseArrayLayer(); arrayLayerIdx < textureView->GetBaseArrayLayer() + textureView->GetArrayLayersNum(); ++arrayLayerIdx)
		{
			for (Uint32 mipLevelIdx = textureView->GetBaseMipLevel(); mipLevelIdx < textureView->GetBaseMipLevel() + textureView->GetMipLevelsNum(); ++mipLevelIdx)
			{
				const RGTextureViewHandle mipView = graphBuilder.CreateTextureMipView(texture, mipLevelIdx, arrayLayerIdx);

				OutputDownload& download = pass.outputs.emplace_back();
				download.name = std::format("{} (Mip {}, Layer {})", texture->GetName().ToString(), mipLevelIdx, arrayLayerIdx);
				download.data = graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("RG Test Mode Download"), mipView).ToSharedPtr();
			}
		}
	}
}

void OutputsHasherDecorator::DownloadBufferOutputs(RenderGraphBuilder& graphBuilder, const RGDependeciesContainer& dependencies, PassDownloads& pass) const
{
	for (const RGBufferAccessDef& bufferAccess : dependencies.bufferAccesses)
	{
		if (bufferAccess.access == ERGBufferAccess::Read)
		{
			continue;
		}

		const RGBufferViewHandle bufferView = bufferAccess.resource;
		const RGBufferHandle buffer         = bufferView->GetBuffer();

		if (buffer->IsNonDeterministic())
		{
			continue;
		}

		// Copy to buffer requires transfer source usage, which is added only to buffers created by render graph
		if (!lib::HasAnyFlag(buffer->GetUsageFlags(), rhi::EBufferUsage::TransferSrc))
		{
			continue;
		}

		OutputDownload& download = pass.outputs.emplace_back();
		download.name = std::format("{} (Offset {}, Size {})", buffer->GetName().ToString(), bufferView->GetOffset(), bufferView->GetSize());
		download.data = graphBuilder.DownloadBuffer(RG_DEBUG_NAME("RG Test Mode Download"), bufferView, 0u, bufferView->GetSize()).ToSharedPtr();
	}
}

lib::Span<PassOutputsHashes> OutputsHasherDecorator::ComputeHashes(lib::MemoryArena& outputScratch) const
{
	SPT_PROFILER_FUNCTION();

	lib::Span<PassOutputsHashes> result = outputScratch.AllocateSpan<PassOutputsHashes>(m_passes.size());

	js::LaunchInline(SPT_GENERIC_JOB_NAME,
					 [this, &result, &outputScratch]
					 {
						for (SizeType passIdx = 0u; passIdx < m_passes.size(); ++passIdx)
						{
							const PassDownloads& pass = m_passes[passIdx];

							PassOutputsHashes& passHashes = result[passIdx];
							passHashes.name = pass.name;

							passHashes.outputs = outputScratch.AllocateSpan<OutputHash>(pass.outputs.size());

							for (SizeType outputIdx = 0u; outputIdx < pass.outputs.size(); ++outputIdx)
							{
								const OutputDownload& output = pass.outputs[outputIdx];
								OutputHash& outputHash = passHashes.outputs[outputIdx];

								js::AddNested(SPT_GENERIC_JOB_NAME,
											  [&output, &outputHash]
											  {
												  const rhi::RHIMappedByteBuffer mappedData(output.data->GetRHI());
												  const lib::Span<const Byte> data = mappedData.GetSpan();

												  outputHash.name = output.name;
												  outputHash.hash = lib::FNV1a::Hash(lib::Span<const char>(reinterpret_cast<const char*>(data.data()), data.size()));
											  });
							}
						}
					});

	return result;
}

} // impl


OutputsHasher::OutputsHasher()
	: m_decorator(lib::MakeShared<impl::OutputsHasherDecorator>())
{ }

void OutputsHasher::Attach(RenderGraphBuilder& graphBuilder)
{
	graphBuilder.AddDebugDecorator(lib::Ref(m_decorator));
}

lib::Span<PassOutputsHashes> OutputsHasher::ComputeHashes(lib::MemoryArena& outputScratch) const
{
	return m_decorator->ComputeHashes(outputScratch);
}

} // spt::rg::test_mode

#endif // RG_ENABLE_TEST_MODE
