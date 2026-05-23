#include "TerrainAsset.h"
#include "AssetsSystem.h"
#include "Engine.h"
#include "TerrainCompiler.h"
#include "Transfers/GPUDeferredCommandsQueueTypes.h"
#include "Transfers/GPUDeferredCommandsQueue.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"


SPT_DEFINE_LOG_CATEGORY(TerrainAsset, true);

namespace spt::as
{

struct TerrainDerivedDataHeader
{
	Uint32 version = 0u;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("version", version);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainAssetInitializer =======================================================================

void TerrainAssetInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<TerrainAssetDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainTexturesUuploadRequest =================================================================

class TerrainTexturesUuploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	TerrainTexturesUuploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void EnqueueUploads() override;
	// End GPUDeferredUploadRequest overrides
	
	lib::SharedPtr<rdr::TextureView> dstHeightMap;
	lib::SharedPtr<rdr::TextureView> dstFarLODBaseColor;
	lib::SharedPtr<rdr::TextureView> dstFarLODProps;
	lib::SharedPtr<rdr::TextureView> dstMaterialIDs;
	lib::MTHandle<DDCLoadedBin>      blob;
};


void TerrainTexturesUuploadRequest::EnqueueUploads()
{
	const CompiledTerrainHeader& terrainDataHeader = reinterpret_cast<const CompiledTerrainHeader&>(*blob->bin.data());

	if (dstHeightMap && terrainDataHeader.heightMap.format != rhi::EFragmentFormat::None)
	{
		const rhi::ETextureAspect textureAspect = dstHeightMap->GetRHI().GetAspect();
		rdr::UploadDataToTexture(blob->bin.data() + terrainDataHeader.heightMap.dataOffset, terrainDataHeader.heightMap.dataSize, dstHeightMap->GetTexture(), textureAspect, dstHeightMap->GetResolution(), math::Vector3u::Zero(), 0u, 0u);
	}

	if (dstFarLODBaseColor && terrainDataHeader.farLODBaseColor.format != rhi::EFragmentFormat::None)
	{
		const rhi::ETextureAspect textureAspect = dstFarLODBaseColor->GetRHI().GetAspect();
		rdr::UploadDataToTexture(blob->bin.data() + terrainDataHeader.farLODBaseColor.dataOffset, terrainDataHeader.farLODBaseColor.dataSize, dstFarLODBaseColor->GetTexture(), textureAspect, dstFarLODBaseColor->GetResolution(), math::Vector3u::Zero(), 0u, 0u);
	}

	if (dstFarLODProps && terrainDataHeader.farLODProps.format != rhi::EFragmentFormat::None)
	{
		const rhi::ETextureAspect textureAspect = dstFarLODProps->GetRHI().GetAspect();
		rdr::UploadDataToTexture(blob->bin.data() + terrainDataHeader.farLODProps.dataOffset, terrainDataHeader.farLODProps.dataSize, dstFarLODProps->GetTexture(), textureAspect, dstFarLODProps->GetResolution(), math::Vector3u::Zero(), 0u, 0u);
	}

	if (dstMaterialIDs && terrainDataHeader.materialIDs.format != rhi::EFragmentFormat::None)
	{
		const rhi::ETextureAspect textureAspect = dstMaterialIDs->GetRHI().GetAspect();
		rdr::UploadDataToTexture(blob->bin.data() + terrainDataHeader.materialIDs.dataOffset, terrainDataHeader.materialIDs.dataSize, dstMaterialIDs->GetTexture(), textureAspect, dstMaterialIDs->GetResolution(), math::Vector3u::Zero(), 0u, 0u);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainAsset ==================================================================================

rsc::TerrainDefinition TerrainAsset::GetTerrainDefinition() const
{
	rsc::TerrainMaterialsMap materialsMap;
	materialsMap.minBounds     = m_terrainMinBounds;
	materialsMap.rcpBoundsSize = (m_terrainMaxBounds - m_terrainMinBounds).cwiseInverse();
	materialsMap.resolution    = m_materialIDs->GetResolution2D().cast<Real32>();
	materialsMap.materialIDs   = m_materialIDs;

	rsc::TerrainDefinition terrainDefinition;
	terrainDefinition.heightMap       = m_heightMap;
	terrainDefinition.farLODBaseColor = m_farLODBaseColor;
	terrainDefinition.farLODProps     = m_farLODProps;
	terrainDefinition.farLODMinBounds = m_terrainMinBounds;
	terrainDefinition.farLODMaxBounds = m_terrainMaxBounds;
	terrainDefinition.material        = m_terrainMaterialAsset.IsValid() ? m_terrainMaterialAsset->GetTerrainMaterialData() : rsc::TerrainMaterialData{};
	terrainDefinition.materialsMap    = materialsMap;


	return terrainDefinition;
}

rsc::TerrainMaterialsMap TerrainAsset::GetTerrainMaterialsMap() const
{
	rsc::TerrainMaterialsMap materialsMap;
	materialsMap.materialIDs = m_materialIDs;
	return materialsMap;
}

Bool TerrainAsset::Compile()
{
	SPT_PROFILER_FUNCTION();

	const TerrainAssetDefinition& definition = GetBlackboard().Get<TerrainAssetDefinition>();

	const std::optional<TerrainCompilationResult> compilationResult = terrain_compiler::CompileTerrain(*this, definition);
	if (!compilationResult.has_value())
	{
		SPT_LOG_ERROR(TerrainAsset, "Failed to compile TerrainAsset '{}'", GetName().ToString());
		return false;
	}

	TerrainDerivedDataHeader header;

	CreateDerivedData(*this, header, compilationResult->blob);

	return true;
}

void TerrainAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<DDCLoadedData<TerrainDerivedDataHeader>> compiledData = LoadDerivedData<TerrainDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	const CompiledTerrainHeader& terrainDataHeader = reinterpret_cast<const CompiledTerrainHeader&>(*compiledData->bin.data());

	if (terrainDataHeader.heightMap.format != rhi::EFragmentFormat::None)
	{
		rhi::TextureDefinition heightMapDef;
		heightMapDef.resolution = terrainDataHeader.heightMap.resolution;
		heightMapDef.format     = terrainDataHeader.heightMap.format;
		heightMapDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		heightMapDef.flags      = rhi::ETextureFlags::GloballyReadable;

		m_heightMap = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("TerrainAsset_HeightMap"), heightMapDef, rhi::EMemoryUsage::GPUOnly);
	}

	if (terrainDataHeader.materialIDs.format != rhi::EFragmentFormat::None)
	{
		rhi::TextureDefinition materialIDsDef;
		materialIDsDef.resolution = terrainDataHeader.materialIDs.resolution;
		materialIDsDef.format     = terrainDataHeader.materialIDs.format;
		materialIDsDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		materialIDsDef.flags      = rhi::ETextureFlags::GloballyReadable;

		m_materialIDs = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("TerrainAsset_MaterialIDs"), materialIDsDef, rhi::EMemoryUsage::GPUOnly);
	}

	if (terrainDataHeader.farLODBaseColor.format != rhi::EFragmentFormat::None)
	{
		rhi::TextureDefinition farLODBaseColorDef;
		farLODBaseColorDef.resolution = terrainDataHeader.farLODBaseColor.resolution;
		farLODBaseColorDef.format     = terrainDataHeader.farLODBaseColor.format;
		farLODBaseColorDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		farLODBaseColorDef.flags      = rhi::ETextureFlags::GloballyReadable;

		m_farLODBaseColor = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("TerrainAsset_FarLODBaseColor"), farLODBaseColorDef, rhi::EMemoryUsage::GPUOnly);
	}

	if (terrainDataHeader.farLODProps.format != rhi::EFragmentFormat::None)
	{
		rhi::TextureDefinition farLODPropsDef;
		farLODPropsDef.resolution = terrainDataHeader.farLODProps.resolution;
		farLODPropsDef.format     = terrainDataHeader.farLODProps.format;
		farLODPropsDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		farLODPropsDef.flags      = rhi::ETextureFlags::GloballyReadable;

		m_farLODProps = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("TerrainAsset_FarLODProps"), farLODPropsDef, rhi::EMemoryUsage::GPUOnly);
	}

	if (terrainDataHeader.terrainMaterialAssetID != InvalidResourcePathID)
	{
		const LoadResult loadRes = GetOwningSystem().LoadAsset<TerrainMaterialAsset>(terrainDataHeader.terrainMaterialAssetID);
		if (loadRes)
		{
			m_terrainMaterialAsset = loadRes.GetValue();
			m_terrainMaterialAsset->AwaitInitialization();
		}
		else
		{
			SPT_LOG_ERROR(TerrainAsset, "Failed to load terrain material asset with path ID {}. Reason: {}", terrainDataHeader.terrainMaterialAssetID, AssetLoadErrorToString(loadRes.GetError()));
		}
	}

	m_terrainMinBounds = terrainDataHeader.terrainMinBounds;
	m_terrainMaxBounds = terrainDataHeader.terrainMaxBounds;

	gfx::GPUDeferredCommandsQueue& commandsQueue = engn::GetEngine().GetPluginsManager().GetPluginChecked<gfx::GPUDeferredCommandsQueue>();

	auto uploadRequest = lib::MakeUnique<TerrainTexturesUuploadRequest>();
	uploadRequest->dstHeightMap       = m_heightMap;
	uploadRequest->dstFarLODBaseColor = m_farLODBaseColor;
	uploadRequest->dstFarLODProps     = m_farLODProps;
	uploadRequest->dstMaterialIDs     = m_materialIDs;
	uploadRequest->blob               = compiledData;

	commandsQueue.RequestUpload(std::move(uploadRequest));
}

} // spt::as
