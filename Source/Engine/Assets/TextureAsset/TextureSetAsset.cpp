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
#include "Transfers/GPUDeferredUploadsQueue.h"


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

void PBRTextureSetAsset::PostCreate()
{
	Super::PostCreate();

	CompileTextureSet();
}

void PBRTextureSetAsset::PostInitialize()
{
	Super::PostInitialize();

	CreateTextureInstances();
}

void PBRTextureSetAsset::CompileTextureSet()
{
	const PBRTextureSetSource& source = GetBlackboard().Get<PBRTextureSetSource>();

	const lib::Path baseColorPath         = (GetOwningSystem().GetContentPath() / source.baseColor.path).generic_string();
	const lib::Path metallicRoughnessPath = (GetOwningSystem().GetContentPath() / source.metallicRoughness.path).generic_string();
	const lib::Path normalsPath           = (GetOwningSystem().GetContentPath() / source.normals.path).generic_string();

	const rhi::ETextureUsage loadedTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::TransferDest);

	const lib::SharedPtr<rdr::Texture> loadedBaseColorTexture         = gfx::TextureLoader::LoadTexture(baseColorPath.generic_string(), loadedTexturesUsage);
	const lib::SharedPtr<rdr::Texture> loadedMetallicRoughnessTexture = gfx::TextureLoader::LoadTexture(metallicRoughnessPath.generic_string(), loadedTexturesUsage);
	const lib::SharedPtr<rdr::Texture> loadedNormalsTexture           = gfx::TextureLoader::LoadTexture(normalsPath.generic_string(), loadedTexturesUsage);

	SPT_CHECK_MSG(!!loadedBaseColorTexture, "Failed to load base color texture");
	SPT_CHECK_MSG(!!loadedMetallicRoughnessTexture, "Failed to load metallic&roughness texture");
	SPT_CHECK_MSG(!!loadedNormalsTexture, "Failed to load normals texture");

	gfx::UploadsManager::Get().FlushPendingUploads();

	PBRTextureSetCompiler textureSetCompiler;
	PBRCompiledTextureSetData compiledData = textureSetCompiler.CompileTextureSet(
	{
		.baseColor         = loadedBaseColorTexture->CreateView(RENDERER_RESOURCE_NAME("Base Color View")).ToSharedPtr(),
		.metallicRoughness = loadedMetallicRoughnessTexture->CreateView(RENDERER_RESOURCE_NAME("Metallic Roughness View")).ToSharedPtr(),
		.normals           = loadedNormalsTexture->CreateView(RENDERER_RESOURCE_NAME("Normals View")).ToSharedPtr()
	}, GetOwningSystem().GetDDC());

	GetBlackboard().Create<PBRCompiledTextureSetData>(std::move(compiledData));
}

void PBRTextureSetAsset::OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)
{
	Super::OnAssetDeleted(assetSystem, path, data);

	if (const PBRCompiledTextureSetData* compiledData = data.blackboard.Find<PBRCompiledTextureSetData>())
	{
		assetSystem.GetDDC().DeleteDerivedData(compiledData->derivedDataKey);
	}
}

void PBRTextureSetAsset::RequestTextureGPUUploads()
{

	const PBRCompiledTextureSetData& compiledData = GetBlackboard().Get<PBRCompiledTextureSetData>();

	gfx::GPUDeferredUploadsQueue& queue = gfx::GPUDeferredUploadsQueue::Get();

	AssetsSystem& assetsSystem = GetOwningSystem();

	const auto createUploadRequest = [&](const lib::SharedPtr<rdr::TextureView>& dstTextureView, const CompiledTexture& compiledTexture)
	{
		lib::UniquePtr<TextureUploadRequest> uploadRequest = lib::MakeUnique<TextureUploadRequest>();
		uploadRequest->dstTextureView  = dstTextureView;
		uploadRequest->compiledTexture = &compiledTexture;
		uploadRequest->ddcKey          = compiledData.derivedDataKey;
		uploadRequest->assetsSystem    = &assetsSystem;
		return uploadRequest;
	};

	queue.RequestUpload(createUploadRequest(m_cachedRuntimeTexture->baseColor, compiledData.baseColor));
	queue.RequestUpload(createUploadRequest(m_cachedRuntimeTexture->metallicRoughness, compiledData.metallicRoughness));
	queue.RequestUpload(createUploadRequest(m_cachedRuntimeTexture->normals, compiledData.normals));

	queue.AddPostDeferredUploadsDelegate(gfx::PostDeferredUploadsMulticastDelegate::Delegate::CreateLambda([self = AssetHandle(this)] // Hard reference is important here
	{
		self->GetBlackboard().Unload<PBRCompiledTextureSetData>();
	}));
}

void PBRTextureSetAsset::CreateTextureInstances()
{
	SPT_PROFILER_FUNCTION();

	const PBRCompiledTextureSetData& textureSetData = GetBlackboard().Get<PBRCompiledTextureSetData>();

	const auto createRHIDefinition = [](const TextureDefinition& def)
	{
		const rhi::ETextureUsage texturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);

		rhi::TextureDefinition outDefinition(def.resolution, texturesUsage, def.format);
		outDefinition.mipLevels = def.mipLevelsNum;

		return outDefinition;
	};

	const lib::SharedPtr<rdr::TextureView>& baseColorTextureView = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Base Color", GetName().ToString()),
																											createRHIDefinition(textureSetData.baseColor.definition),
																											rhi::EMemoryUsage::GPUOnly);

	const lib::SharedPtr<rdr::TextureView>& metallicRoughnessTextureView = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Metallic Roughness", GetName().ToString()),
																													createRHIDefinition(textureSetData.metallicRoughness.definition),
																													rhi::EMemoryUsage::GPUOnly);

	const lib::SharedPtr<rdr::TextureView>& normalsTextureView = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{} Normals", GetName().ToString()),
																										  createRHIDefinition(textureSetData.normals.definition),
																										  rhi::EMemoryUsage::GPUOnly);

	RuntimePBRTextureSetData& textures = GetBlackboard().Create<RuntimePBRTextureSetData>(RuntimePBRTextureSetData
	{
		.baseColor         = baseColorTextureView,
		.metallicRoughness = metallicRoughnessTextureView,
		.normals           = normalsTextureView
	});

	m_cachedRuntimeTexture = &textures;
}

} // spt::as
