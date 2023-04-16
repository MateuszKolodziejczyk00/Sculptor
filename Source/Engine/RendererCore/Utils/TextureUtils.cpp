#include "TextureUtils.h"
#include "Types/Texture.h"
#include "CommandsRecorder/CommandRecorder.h"

namespace spt::rdr
{

namespace utils
{

void GenerateMipMaps(rdr::CommandRecorder& recorder, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, Uint32 arrayLayer, const rhi::BarrierTextureTransitionDefinition& destTransition)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHITexture& rhiTexture = texture->GetRHI();
	const rhi::TextureDefinition& textureDef = rhiTexture.GetDefinition();
	const Uint32 mipLevels = static_cast<Uint32>(textureDef.mipLevels);

	SPT_CHECK(mipLevels > 1);

	for (Uint32 idx = 1; idx < mipLevels; ++idx)
	{
		const Uint32 sourceMip = idx - 1;
		const Uint32 destMip = idx;

		rhi::RHIDependency dependency;

		const SizeType sourceDependencyIdx = dependency.AddTextureDependency(rhiTexture, rhi::TextureSubresourceRange(aspect, sourceMip, 1, arrayLayer, 1));
		dependency.SetLayoutTransition(sourceDependencyIdx, rhi::TextureTransition::TransferSource);
		
		const SizeType destDependencyIdx = dependency.AddTextureDependency(rhiTexture, rhi::TextureSubresourceRange(aspect, destMip, 1, arrayLayer, 1));
		dependency.SetLayoutTransition(destDependencyIdx, rhi::TextureTransition::TransferDest);

		if (idx >= 2)
		{
			const Uint32 unusedMip = static_cast<Uint32>(idx - 2);
			const SizeType unusedDependencyIdx = dependency.AddTextureDependency(rhiTexture, rhi::TextureSubresourceRange(aspect, unusedMip, 1, arrayLayer, 1));
			dependency.SetLayoutTransition(unusedDependencyIdx, destTransition);
		}

		recorder.ExecuteBarrier(dependency);

		recorder.BlitTexture(texture, sourceMip, arrayLayer, texture, destMip, arrayLayer, aspect, rhi::ESamplerFilterType::Linear);
	}

	rhi::RHIDependency destDependency;

	for (SizeType mip = mipLevels - 2; mip < mipLevels; ++mip)
	{
		const SizeType depIdx = destDependency.AddTextureDependency(rhiTexture, rhi::TextureSubresourceRange(aspect, static_cast<Uint32>(mip), 1, arrayLayer, 1));
		destDependency.SetLayoutTransition(depIdx, destTransition);
	}

	recorder.ExecuteBarrier(destDependency);
}

} // utils

} // spt::rdr
