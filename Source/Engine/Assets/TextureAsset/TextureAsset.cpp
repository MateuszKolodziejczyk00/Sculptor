#include "TextureAsset.h"
#include "ResourcesManager.h"
#include "RendererResource.h"
#include "TextureStreamer.h"
#include "Types/Texture.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "Transfers/UploadUtils.h"

namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureDataInitializer ========================================================================

void TextureDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<TextureDefinition>(m_definition);
	asset.GetBlackboard().Create<TextureData>(m_data);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureAsset ==================================================================================

void TextureAsset::PostInitialize()
{
	Super::PostInitialize();

	CreateTextureInstance();

	TextureStreamer::Get().RequestTextureUploadToGPU(StreamRequest
													 {
														 .assetHandle       = this,
														 .streamableTexture = this
													 });
}

void TextureAsset::PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency)
{
	const lib::SharedPtr<rdr::TextureView>& textureView = GetTextureView();

	const SizeType dependencyIdx = preUploadDependency.AddTextureDependency(textureView->GetTexture()->GetRHI(), textureView->GetRHI().GetSubresourceRange());
	preUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::TransferDest);
}

void TextureAsset::ScheduleUploads()
{
	SPT_PROFILER_FUNCTION();

	const TextureData& textureData = GetBlackboard().Get<TextureData>();

	const lib::SharedPtr<rdr::TextureView>& textureView = GetTextureView();
	const lib::SharedRef<rdr::Texture>& texture         = textureView->GetTexture();

	const Uint32 mipLevelsNum = texture->GetRHI().GetDefinition().mipLevels;
	SPT_CHECK_MSG(mipLevelsNum == textureData.mips.size(), "Mismatch between texture definition and data");

	const rhi::ETextureAspect textureAspect = textureView->GetRHI().GetAspect();

	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < mipLevelsNum; ++mipLevelIdx)
	{
		const TextureMipData& mip = textureData.mips[mipLevelIdx];
		gfx::UploadDataToTexture(mip.data.data(), mip.data.size(), texture, textureAspect, texture->GetRHI().GetMipResolution(mipLevelIdx), math::Vector3u::Zero(), mipLevelIdx, 0u);
	}
}

void TextureAsset::FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency)
{
	const lib::SharedPtr<rdr::TextureView>& textureView = GetTextureView();

	const SizeType dependencyIdx = postUploadDependency.AddTextureDependency(textureView->GetTexture()->GetRHI(), textureView->GetRHI().GetSubresourceRange());
	postUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::ReadOnly);

	GetBlackboard().Unload(lib::TypeInfo<TextureData>());
}

void TextureAsset::CreateTextureInstance()
{
	SPT_PROFILER_FUNCTION();

	const TextureDefinition& definition = GetBlackboard().Get<TextureDefinition>();

	const rhi::ETextureUsage usageFlags = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);

	rhi::TextureDefinition textureDefinition(definition.resolution, usageFlags, definition.format);
	textureDefinition.mipLevels = definition.mipLevelsNum;

	const lib::SharedPtr<rdr::TextureView> textureInstance = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME(GetName()), textureDefinition, rhi::EMemoryUsage::GPUOnly);
	SPT_CHECK_MSG(textureInstance, "Failed to create texture instance");

	RuntimeTexture& runtimeTextureComponent = GetBlackboard().Create<RuntimeTexture>(RuntimeTexture{ textureInstance });

	m_cachedRuntimeTexture = &runtimeTextureComponent;
}

} // spt::as
