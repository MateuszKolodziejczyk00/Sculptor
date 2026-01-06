#include "TextureSetAsset.h"
#include "Loaders/TextureLoader.h"
#include "Paths.h"
#include "PBRTextureSetCompiler.h"
#include "Types/Texture.h"
#include "AssetsSystem.h"
#include "Transfers/UploadsManager.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"
#include "Transfers/GPUDeferredCommandsQueue.h"


namespace spt::as
{

//================================================================================================
// PBRTextureSetInitializer ======================================================================

void PBRTextureSetInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<PBRTextureSetSource>(std::move(m_source));
}

//================================================================================================
// PBRTextureSetAsset ============================================================================

Bool PBRTextureSetAsset::Compile()
{
	return CompileTextureSet();
}

void PBRTextureSetAsset::PostInitialize()
{
	Super::PostInitialize();

	CreateTextureInstances();
}

Bool PBRTextureSetAsset::CompileTextureSet()
{
	SPT_PROFILER_FUNCTION();

	const PBRTextureSetSource& source = GetBlackboard().Get<PBRTextureSetSource>();

	PBRTextureSetCompiler textureSetCompiler;
	const std::optional<PBRCompiledTextureSetData> compiledData = textureSetCompiler.CompileTextureSet(this, source);

	if (!compiledData.has_value())
	{
		return false;
	}

	CreateDerivedData(*this, compiledData->compiledSet, lib::Span<const Byte>(compiledData->texturesData.data(), compiledData->texturesData.size()));

	return true;
}

void PBRTextureSetAsset::CreateTextureInstances()
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<DDCLoadedData<CompiledPBRTextureSet>> dd = LoadDerivedData<CompiledPBRTextureSet>(*this);

	const auto createRHIDefinition = [](const TextureDefinition& def)
	{
		const rhi::ETextureUsage usageFlags = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		rhi::TextureDefinition outDefinition(def.resolution, usageFlags, def.format);
		outDefinition.mipLevels = def.mipLevelsNum;
		return outDefinition;
	};

	m_baseColor         = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Base Color", GetName().ToString()), createRHIDefinition(dd->header.baseColor.definition), rhi::EMemoryUsage::GPUOnly);
	m_metallicRoughness = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Metallic Roughness", GetName().ToString()), createRHIDefinition(dd->header.metallicRoughness.definition), rhi::EMemoryUsage::GPUOnly);
	m_normals           = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Normals", GetName().ToString()), createRHIDefinition(dd->header.normals.definition), rhi::EMemoryUsage::GPUOnly);

	const auto createUploadRequest = [&](const lib::SharedPtr<rdr::TextureView>& dstTextureView, const CompiledTexture& compiledTexture)
	{
		lib::UniquePtr<TextureUploadRequest> uploadRequest = lib::MakeUnique<TextureUploadRequest>();
		uploadRequest->dstTextureView  = dstTextureView;
		uploadRequest->compiledTexture = &compiledTexture;
		uploadRequest->textureData     = dd->bin;
		return uploadRequest;
	};

	gfx::GPUDeferredCommandsQueue& queue = gfx::GPUDeferredCommandsQueue::Get();

	queue.RequestUpload(createUploadRequest(m_baseColor, dd->header.baseColor));
	queue.RequestUpload(createUploadRequest(m_metallicRoughness, dd->header.metallicRoughness));
	queue.RequestUpload(createUploadRequest(m_normals, dd->header.normals));

	queue.AddPostDeferredUploadsDelegate(gfx::PostDeferredUploadsMulticastDelegate::Delegate::CreateLambda([dd] {})); // Keep reference to loaded derived data until uploads are finished
}

} // spt::as
