#include "TextureAsset.h"
#include "ResourcesManager.h"
#include "RendererResource.h"
#include "Types/Texture.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "Transfers/UploadUtils.h"
#include "TextureCompiler.h"
#include "AssetsSystem.h"
#include "Transfers/GPUDeferredUploadsQueue.h"

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

	const CompiledTextureData& compiledData = GetBlackboard().Get<CompiledTextureData>();

	const AssetsSystem& assetsSystem = GetOwningSystem();

	gfx::GPUDeferredUploadsQueue& queue = gfx::GPUDeferredUploadsQueue::Get();

	lib::UniquePtr<TextureUploadRequest> uploadRequest = lib::MakeUnique<TextureUploadRequest>();
	uploadRequest->dstTextureView  = m_cachedRuntimeTexture->textureInstance;
	uploadRequest->compiledTexture = &compiledData.texture;
	uploadRequest->ddcKey          = compiledData.derivedDataKey;
	uploadRequest->assetsSystem    = &assetsSystem;

	queue.RequestUpload(std::move(uploadRequest));

	queue.AddPostDeferredUploadsDelegate(gfx::PostDeferredUploadsMulticastDelegate::Delegate::CreateLambda([self = AssetHandle(this)] // Hard reference is important here
	{
		self->GetBlackboard().Unload<CompiledTextureData>();
	}));
}

void TextureAsset::OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)
{
	Super::OnAssetDeleted(assetSystem, path, data);

	if (const CompiledTextureData* compiledData = data.blackboard.Find<CompiledTextureData>())
	{
		assetSystem.GetDDC().DeleteDerivedData(compiledData->derivedDataKey);
	}
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
