#include "TextureAsset.h"
#include "ResourcesManager.h"
#include "RendererResource.h"
#include "Types/Texture.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "Transfers/UploadUtils.h"
#include "TextureCompiler.h"
#include "AssetsSystem.h"
#include "Transfers/GPUDeferredCommandsQueue.h"

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

Bool TextureAsset::Compile()
{
	return CompileTexture();
}

void TextureAsset::PostInitialize()
{
	Super::PostInitialize();

	CreateTextureInstance();
}

Bool TextureAsset::CompileTexture()
{
	SPT_PROFILER_FUNCTION();

	const TextureSourceDefinition& source = GetBlackboard().Get<TextureSourceDefinition>();

	TextureCompiler compiler;
	std::optional<TextureCompilationResult> compilationRes = compiler.CompileTexture(*this, source);

	if(!compilationRes.has_value())
	{
		return false;
	}

	CreateDerivedData(*this, compilationRes->compiledTexture, lib::Span<const Byte>(compilationRes->textureData.data(), compilationRes->textureData.size()));

	return true;
}

void TextureAsset::CreateTextureInstance()
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<DDCLoadedData<CompiledTexture>> dd = LoadDerivedData<CompiledTexture>(*this);
	SPT_CHECK(dd.IsValid());

	const TextureDefinition& definition = dd->header.definition;

	const rhi::ETextureUsage usageFlags = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);

	rhi::TextureDefinition textureDefinition(definition.resolution, usageFlags, definition.format);
	textureDefinition.mipLevels = definition.mipLevelsNum;

	m_textureInstance = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME(GetName()), textureDefinition, rhi::EMemoryUsage::GPUOnly);
	SPT_CHECK_MSG(!!m_textureInstance, "Failed to create texture instance");

	gfx::GPUDeferredCommandsQueue& queue = gfx::GPUDeferredCommandsQueue::Get();

	lib::UniquePtr<TextureUploadRequest> uploadRequest = lib::MakeUnique<TextureUploadRequest>();
	uploadRequest->dstTextureView  = m_textureInstance;
	uploadRequest->compiledTexture = &dd->header;
	uploadRequest->textureData     = dd->bin;

	queue.RequestUpload(std::move(uploadRequest));

	// Keep reference to loaded derived data until upload is finished
	queue.AddPostDeferredUploadsDelegate(gfx::PostDeferredUploadsMulticastDelegate::Delegate::CreateLambda([dd] {}));
}

} // spt::as
