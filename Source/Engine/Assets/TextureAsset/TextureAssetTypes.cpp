#include "TextureAssetTypes.h"


namespace spt::as
{

void TextureUploadRequest::PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency)
{
	const SizeType dependencyIdx = preUploadDependency.AddTextureDependency(dstTextureView->GetTexture()->GetRHI(), dstTextureView->GetRHI().GetSubresourceRange());
	preUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::TransferDest);
}

void TextureUploadRequest::EnqueueUploads()
{
	const lib::SharedRef<rdr::Texture>& texture = dstTextureView->GetTexture();

	const Uint32 mipLevelsNum = texture->GetRHI().GetDefinition().mipLevels;
	SPT_CHECK_MSG(mipLevelsNum == compiledTexture->mips.size(), "Mismatch between texture definition and data");

	const rhi::ETextureAspect textureAspect = dstTextureView->GetRHI().GetAspect();

	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < mipLevelsNum; ++mipLevelIdx)
	{
		const CompiledMip& mip = compiledTexture->mips[mipLevelIdx];

		SPT_CHECK(mip.offset + mip.size <= textureData.size());
		gfx::UploadDataToTexture(textureData.data() + mip.offset, mip.size, texture, textureAspect, texture->GetRHI().GetMipResolution(mipLevelIdx), math::Vector3u::Zero(), mipLevelIdx, 0u);
	}
}

void TextureUploadRequest::FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency)
{
	const SizeType dependencyIdx = postUploadDependency.AddTextureDependency(dstTextureView->GetTexture()->GetRHI(), dstTextureView->GetRHI().GetSubresourceRange());
	postUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::ShaderRead);
}

} // spt::as
