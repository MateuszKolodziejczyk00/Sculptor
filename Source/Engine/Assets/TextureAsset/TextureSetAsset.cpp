#include "TextureSetAsset.h"
#include "Loaders/TextureLoader.h"
#include "Paths.h"
#include "PBRTextureSetCompiler.h"
#include "Types/Texture.h"
#include "AssetsSystem.h"
#include "Transfers/UploadsManager.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "TextureStreamer.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"


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

	TextureStreamer::Get().RequestTextureUploadToGPU(StreamRequest
													 {
														 .assetHandle       = this,
														 .streamableTexture = this
													 });
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

void PBRTextureSetAsset::PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency)
{
	const lib::SharedPtr<rdr::TextureView>& baseColorTextureView         = GetBaseColorTextureView();
	const lib::SharedPtr<rdr::TextureView>& metallicRoughnessTextureView = GetMetallicRoughnessTextureView();
	const lib::SharedPtr<rdr::TextureView>& normalsTextureView           = GetNormalsTextureView();

	const SizeType baseColorDependencyIdx         = preUploadDependency.AddTextureDependency(baseColorTextureView->GetTexture()->GetRHI(), baseColorTextureView->GetRHI().GetSubresourceRange());
	const SizeType metallicRoughnessDependencyIdx = preUploadDependency.AddTextureDependency(metallicRoughnessTextureView->GetTexture()->GetRHI(), metallicRoughnessTextureView->GetRHI().GetSubresourceRange());
	const SizeType normalsDependencyIdx           = preUploadDependency.AddTextureDependency(normalsTextureView->GetTexture()->GetRHI(), normalsTextureView->GetRHI().GetSubresourceRange());

	preUploadDependency.SetLayoutTransition(baseColorDependencyIdx,         rhi::TextureTransition::TransferDest);
	preUploadDependency.SetLayoutTransition(metallicRoughnessDependencyIdx, rhi::TextureTransition::TransferDest);
	preUploadDependency.SetLayoutTransition(normalsDependencyIdx,           rhi::TextureTransition::TransferDest);
}

void PBRTextureSetAsset::ScheduleUploads()
{
	const auto scheduleTextureUpload = [](const lib::SharedPtr<rdr::TextureView>& textureView, const CompiledTexture& compiledTexture, lib::Span<const Byte> derivedData)
	{
		const lib::SharedRef<rdr::Texture>& textureInstance = textureView->GetTexture();

		const Uint32 mipLevelsNum = textureInstance->GetRHI().GetDefinition().mipLevels;
		
		SPT_CHECK_MSG(mipLevelsNum == compiledTexture.mips.size(), "Mismatch between texture definition and data");

		const rhi::ETextureAspect textureAspect = textureView->GetRHI().GetAspect();

		for (Uint32 mipLevelIdx = 0u; mipLevelIdx < mipLevelsNum; ++mipLevelIdx)
		{
			const CompiledMip& mip = compiledTexture.mips[mipLevelIdx];

			SPT_CHECK(mip.offset + mip.size <= derivedData.size());

			gfx::UploadDataToTexture(derivedData.data() + mip.offset, mip.size, textureInstance, textureAspect, textureInstance->GetRHI().GetMipResolution(mipLevelIdx), math::Vector3u::Zero(), mipLevelIdx, 0u);
		}
	};

	const PBRCompiledTextureSetData& compiledTextureSet = GetBlackboard().Get<PBRCompiledTextureSetData>();

	const DDCResourceHandle derivedDataHandle = GetOwningSystem().GetDDC().GetResourceHandle(compiledTextureSet.derivedDataKey);
	const lib::Span<const Byte> derivedData = derivedDataHandle.GetImmutableSpan();

	const lib::SharedPtr<rdr::TextureView>& baseColorTextureView         = GetBaseColorTextureView();
	const lib::SharedPtr<rdr::TextureView>& metallicRoughnessTextureView = GetMetallicRoughnessTextureView();
	const lib::SharedPtr<rdr::TextureView>& normalsTextureView           = GetNormalsTextureView();

	scheduleTextureUpload(baseColorTextureView, compiledTextureSet.baseColor, derivedData);
	scheduleTextureUpload(metallicRoughnessTextureView, compiledTextureSet.metallicRoughness, derivedData);
	scheduleTextureUpload(normalsTextureView, compiledTextureSet.normals, derivedData);
}

void PBRTextureSetAsset::FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency)
{
	const lib::SharedPtr<rdr::TextureView>& baseColorTextureView         = GetBaseColorTextureView();
	const lib::SharedPtr<rdr::TextureView>& metallicRoughnessTextureView = GetMetallicRoughnessTextureView();
	const lib::SharedPtr<rdr::TextureView>& normalsTextureView           = GetNormalsTextureView();

	const SizeType baseColorDependencyIdx         = postUploadDependency.AddTextureDependency(baseColorTextureView->GetTexture()->GetRHI(), baseColorTextureView->GetRHI().GetSubresourceRange());
	const SizeType metallicRoughnessDependencyIdx = postUploadDependency.AddTextureDependency(metallicRoughnessTextureView->GetTexture()->GetRHI(), metallicRoughnessTextureView->GetRHI().GetSubresourceRange());
	const SizeType normalsDependencyIdx           = postUploadDependency.AddTextureDependency(normalsTextureView->GetTexture()->GetRHI(), normalsTextureView->GetRHI().GetSubresourceRange());

	postUploadDependency.SetLayoutTransition(baseColorDependencyIdx,         rhi::TextureTransition::ReadOnly);
	postUploadDependency.SetLayoutTransition(metallicRoughnessDependencyIdx, rhi::TextureTransition::ReadOnly);
	postUploadDependency.SetLayoutTransition(normalsDependencyIdx,           rhi::TextureTransition::ReadOnly);

	GetBlackboard().Unload(lib::TypeInfo<PBRCompiledTextureSetData>());
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
