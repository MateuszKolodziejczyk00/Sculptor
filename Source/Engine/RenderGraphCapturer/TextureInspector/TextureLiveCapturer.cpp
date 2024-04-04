#include "TextureLiveCapturer.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"


namespace spt::rg::capture
{

TextureLiveCapturer::TextureLiveCapturer(const LiveCaptureDestTexture& destTexture)
	: m_destTexture(destTexture)
{
}

void TextureLiveCapturer::PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies)
{
	if (node.GetName().Get() == m_destTexture.passName)
	{
		for (const RGTextureAccessDef& textureAccess : dependencies.textureAccesses)
		{
			if (textureAccess.textureView->GetName() == m_destTexture.textureName)
			{
				const RGTextureViewHandle rgTextureView = textureAccess.textureView;
				const RGTextureHandle rgTexture = rgTextureView->GetTexture();

				rhi::TextureDefinition captureTextureDef;
				lib::AddFlags(captureTextureDef.usage, lib::Flags(rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::SampledTexture));
				captureTextureDef.resolution	= rgTextureView->GetResolution();
				captureTextureDef.format		= rgTextureView->GetFormat();
				captureTextureDef.mipLevels		= 1u;
				captureTextureDef.type			= rhi::ETextureType::Texture2D;
				const lib::SharedRef<rdr::TextureView> capturedTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME(rgTexture->GetName()),
																												  captureTextureDef,
																												  rhi::EMemoryUsage::GPUOnly);

				const RGTextureViewHandle rgTextureCapture = graphBuilder.AcquireExternalTextureView(capturedTexture.ToSharedPtr());

				graphBuilder.CopyFullTexture(RG_DEBUG_NAME("RG Capture Copy"), rgTextureView, rgTextureCapture);

				js::Launch(SPT_GENERIC_JOB_NAME,
						   [texture = capturedTexture.ToSharedPtr(), destTextureOutput = m_destTexture.output]()
						   {
							   destTextureOutput->SetTexture(std::move(texture));
						   },
						   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
			}
		}
	}
}

} // spt::rg::capture
