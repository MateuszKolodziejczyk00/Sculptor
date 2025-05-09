#include "TextureAsset.h"
#include "ResourcesManager.h"
#include "RendererResource.h"
#include "TextureStreamer.h"
#include "Types/Texture.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "Transfers/UploadUtils.h"
#include "TextureCompiler.h"
#include "AssetsSystem.h"

namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureDataInitializer ========================================================================

void TextureDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<TextureSourceDefinition>(std::move(m_source));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureAsset ==================================================================================

void TextureAsset::PostCreate()
{
	Super::PostCreate();

	CompileTexture();
}

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

void TextureAsset::OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)
{
	Super::OnAssetDeleted(assetSystem, path, data);

	if (const CompiledTextureData* compiledData = data.blackboard.Find<CompiledTextureData>())
	{
		assetSystem.GetDDC().DeleteDerivedData(compiledData->derivedDataKey);
	}
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

	const CompiledTextureData& compiledTexture = GetBlackboard().Get<CompiledTextureData>();

	const lib::SharedPtr<rdr::TextureView>& textureView = GetTextureView();
	const lib::SharedRef<rdr::Texture>& texture         = textureView->GetTexture();

	const Uint32 mipLevelsNum = texture->GetRHI().GetDefinition().mipLevels;
	SPT_CHECK_MSG(mipLevelsNum == compiledTexture.texture.mips.size(), "Mismatch between texture definition and data");

	const DDCResourceHandle derivedDataHandle = GetOwningSystem().GetDDC().GetResourceHandle(compiledTexture.derivedDataKey);
	const lib::Span<const Byte> derivedData = derivedDataHandle.GetImmutableSpan();

	const rhi::ETextureAspect textureAspect = textureView->GetRHI().GetAspect();

	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < mipLevelsNum; ++mipLevelIdx)
	{
		const CompiledMip& mip = compiledTexture.texture.mips[mipLevelIdx];

		SPT_CHECK(mip.offset + mip.size <= derivedData.size());
		gfx::UploadDataToTexture(derivedData.data() + mip.offset, mip.size, texture, textureAspect, texture->GetRHI().GetMipResolution(mipLevelIdx), math::Vector3u::Zero(), mipLevelIdx, 0u);
	}
}

void TextureAsset::FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency)
{
	const lib::SharedPtr<rdr::TextureView>& textureView = GetTextureView();

	const SizeType dependencyIdx = postUploadDependency.AddTextureDependency(textureView->GetTexture()->GetRHI(), textureView->GetRHI().GetSubresourceRange());
	postUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::ReadOnly);

	GetBlackboard().Unload(lib::TypeInfo<CompiledTextureData>());
}

void TextureAsset::CompileTexture()
{
	const TextureSourceDefinition& source = GetBlackboard().Get<TextureSourceDefinition>();

	const DDC& ddc = GetOwningSystem().GetDDC();

	TextureCompiler compiler;
	CompiledTextureData compiledTexture = compiler.CompileTexture(source, *this, ddc);

	SPT_CHECK_MSG(compiledTexture.texture.mips.size() > 0u, "Failed to compile texture");

	GetBlackboard().Create<CompiledTextureData>(std::move(compiledTexture));
}

void TextureAsset::CreateTextureInstance()
{
	SPT_PROFILER_FUNCTION();

	const CompiledTextureData& compiledTexture = GetBlackboard().Get<CompiledTextureData>();

	const TextureDefinition& definition = compiledTexture.texture.definition;

	const rhi::ETextureUsage usageFlags = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);

	rhi::TextureDefinition textureDefinition(definition.resolution, usageFlags, definition.format);
	textureDefinition.mipLevels = definition.mipLevelsNum;

	const lib::SharedPtr<rdr::TextureView> textureInstance = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME(GetName()), textureDefinition, rhi::EMemoryUsage::GPUOnly);
	SPT_CHECK_MSG(textureInstance, "Failed to create texture instance");

	RuntimeTexture& runtimeTextureComponent = GetBlackboard().Create<RuntimeTexture>(RuntimeTexture{ textureInstance });

	m_cachedRuntimeTexture = &runtimeTextureComponent;
}

} // spt::as
