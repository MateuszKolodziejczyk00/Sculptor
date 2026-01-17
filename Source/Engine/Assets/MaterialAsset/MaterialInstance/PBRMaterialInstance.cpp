#include "PBRMaterialInstance.h"
#include "Assertions/Assertions.h"
#include "MaterialAsset.h"
#include "MaterialInstance/PBRMaterialCompiler.h"
#include "MaterialInstance/PBRMaterialRuntime.h"
#include "MaterialsSubsystem.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RendererUtils.h"
#include "ResourcesManager.h"
#include "Transfers/GPUDeferredCommandsQueue.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// PBRMaterialInitializer ========================================================================

void PBRMaterialInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<PBRMaterialDefinition>(m_definition);

	MaterialTypeInfo materialTypeInfo;
	materialTypeInfo.materialType = lib::TypeInfo<PBRMaterialInstance>();
	asset.GetBlackboard().Create<MaterialTypeInfo>(std::move(materialTypeInfo));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PBRGLTFMaterialInitializer ========================================================================

void PBRGLTFMaterialInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<PBRGLTFMaterialDefinition>(m_definition);

	MaterialTypeInfo materialTypeInfo;
	materialTypeInfo.materialType = lib::TypeInfo<PBRMaterialInstance>();
	asset.GetBlackboard().Create<MaterialTypeInfo>(std::move(materialTypeInfo));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PBRMaterialInstance ===========================================================================

lib::DynamicArray<Byte> PBRMaterialInstance::Compile(const AssetInstance& asset)
{
	SPT_PROFILER_FUNCTION();
	
	if(const PBRMaterialDefinition* materialDefinition = asset.GetBlackboard().Find<PBRMaterialDefinition>())
	{
		return material_compiler::CompilePBRMaterial(asset, *materialDefinition);
	}
	else if (const PBRGLTFMaterialDefinition* gltfDefinition = asset.GetBlackboard().Find<PBRGLTFMaterialDefinition>())
	{
		return material_compiler::CompilePBRMaterial(asset, *gltfDefinition);
	}
	else
	{
		SPT_CHECK_NO_ENTRY();
		return {};
	}
}

void PBRMaterialInstance::Load(const AssetInstance& asset, lib::MTHandle<DDCLoadedBin> loadedBlob)
{
	SPT_PROFILER_FUNCTION();

	const PBRMaterialDataHeader& materialDataHeader = reinterpret_cast<const PBRMaterialDataHeader&>(*loadedBlob->bin.data());

	const auto initTexture = [loadedBlob](const MaterialTextureDefinition& textureDef) -> lib::SharedPtr<rdr::TextureView>
	{
		if (textureDef.format == rhi::EFragmentFormat::None)
		{
			return nullptr;
		}

		SPT_CHECK(textureDef.resolution.x() > 0u && textureDef.resolution.y() > 0u);
		SPT_CHECK(textureDef.mipLevelsNum > 0u);

		rhi::TextureDefinition gpuTextureDef;
		gpuTextureDef.resolution = textureDef.resolution;
		gpuTextureDef.format     = textureDef.format;
		gpuTextureDef.mipLevels  = textureDef.mipLevelsNum;
		gpuTextureDef.type       = rhi::ETextureType::Texture2D;
		gpuTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		lib::SharedPtr<rdr::TextureView> gpuTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("TEMP"), gpuTextureDef, rhi::EMemoryUsage::GPUOnly);

		gfx::GPUDeferredCommandsQueue& commandsQueue = gfx::GPUDeferredCommandsQueue::Get();

		auto uploadRequest = lib::MakeUnique<MaterialTextureUploadRequest>();
		uploadRequest->dstTextureView = gpuTexture;
		uploadRequest->textureDef     = &textureDef;
		uploadRequest->blob           = loadedBlob;

		commandsQueue.RequestUpload(std::move(uploadRequest));

		return gpuTexture;
	};

	m_materialData = {};
	m_materialData.baseColorFactor          = materialDataHeader.baseColorFactor;
	m_materialData.metallicFactor           = materialDataHeader.metallicFactor;
	m_materialData.roughnessFactor          = materialDataHeader.roughnessFactor;
	m_materialData.emissiveFactor           = materialDataHeader.emissionFactor;
	m_materialData.baseColorTexture         = initTexture(materialDataHeader.baseColorTexture);
	m_materialData.metallicRoughnessTexture = initTexture(materialDataHeader.metallicRoughnessTexture);
	m_materialData.normalsTexture           = initTexture(materialDataHeader.normalsTexture);
	m_materialData.emissiveTexture          = initTexture(materialDataHeader.emissiveTexture);

	const Bool isEmissive = materialDataHeader.emissionFactor.x() > 0.f || materialDataHeader.emissionFactor.y() || materialDataHeader.emissionFactor.z();

	mat::MaterialDefinition materialDefinition;
	materialDefinition.name          = asset.GetName();
	materialDefinition.customOpacity = materialDataHeader.customOpacity != 0u;
	materialDefinition.transparent   = false;
	materialDefinition.doubleSided   = materialDataHeader.doubleSided != 0u;
	materialDefinition.emissive      = isEmissive;

	m_materialEntity = mat::MaterialsSubsystem::Get().CreateMaterial(materialDefinition, m_materialData);
}

} // spt::as
