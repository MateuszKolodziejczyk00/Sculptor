#include "CompiledMaterial.h"
#include "AssetTypes.h"
#include "Utils/TransfersUtils.h"
#include "Types/Texture.h"


namespace spt::as
{

void MaterialTextureUploadRequest::EnqueueUploads()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::Texture>& texture = dstTextureView->GetTexture();

	const Uint32 mipLevelsNum = texture->GetRHI().GetDefinition().mipLevels;
	SPT_CHECK_MSG(mipLevelsNum == textureDef->mipLevelsNum, "Mismatch between texture definition and data");

	const rhi::ETextureAspect textureAspect = dstTextureView->GetRHI().GetAspect();

	const lib::Span<const Byte> textureData =  blob->bin;

	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < mipLevelsNum; ++mipLevelIdx)
	{
		const MaterialTextureMipDefinition& mip = textureDef->mipLevels[mipLevelIdx];

		SPT_CHECK(mip.dataOffset + mip.dataSize <= textureData.size());
		rdr::UploadDataToTexture(textureData.data() + mip.dataOffset, mip.dataSize, texture, textureAspect, texture->GetRHI().GetMipResolution(mipLevelIdx), math::Vector3u::Zero(), mipLevelIdx, 0u);
	}
}

} // spt::as
