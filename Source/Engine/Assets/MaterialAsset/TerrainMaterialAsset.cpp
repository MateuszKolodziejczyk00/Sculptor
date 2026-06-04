#include "TerrainMaterialAsset.h"
#include "AssetsSystem.h"
#include "ResourcesManager.h"
#include "Utils/TransfersManager.h"


SPT_DEFINE_LOG_CATEGORY(TerrainMaterialAsset, true);

namespace spt::as
{

struct TerrainCompiledMaterialEntry
{
	ResourcePathID materialAssetPathID = InvalidResourcePathID;
	Real32         uvScale             = 1.f;
	Uint32         grassType           = idxNone<Uint32>;
};


struct TerrainMaterialDerivedData
{
	lib::StaticArray<TerrainCompiledMaterialEntry, rsc::terrain_material_props::maxMaterialEntries> compiledEntries;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainMaterialInitializer ====================================================================

void TerrainMaterialInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<TerrainMaterialDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TerrainMaterialAsset ===========================================================================

const rsc::TerrainMaterialData& TerrainMaterialAsset::GetTerrainMaterialData() const
{
	return m_terrainMaterialData;
}

Bool TerrainMaterialAsset::Compile()
{
	const TerrainMaterialDefinition& definition = GetBlackboard().Get<TerrainMaterialDefinition>();

	TerrainMaterialDerivedData dd;

	for (SizeType i = 0; i < definition.materialEntries.GetSize(); ++i)
	{
		const TerrainMaterialEntry& materialEntry = definition.materialEntries[i];
		if (!materialEntry.materialAsset.IsValid())
		{
			continue;
		}

		const ResourcePath materialAssetPath = ResolveAssetRelativePath(materialEntry.materialAsset.GetPath());
		if (!GetOwningSystem().CompileAssetIfDeprecated(materialAssetPath))
		{
			SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to compile TerrainMaterialAsset '{}' - referenced MaterialAsset '{}' could not be compiled", GetName().ToString(), materialAssetPath.GetPath().generic_string());
			return false;
		}

		TerrainCompiledMaterialEntry& compiledEntry = dd.compiledEntries[i];
		compiledEntry.materialAssetPathID = materialAssetPath.GetID();
		compiledEntry.uvScale             = 1.f / materialEntry.uvTileMeters;
		compiledEntry.grassType           = materialEntry.grassType;
	}

	CreateDerivedData(*this, DDCNoHeader{}, lib::Span<const Byte>(reinterpret_cast<const Byte*>(&dd), sizeof(TerrainMaterialDerivedData)));

	return true;
}

void TerrainMaterialAsset::OnInitialize()
{
	const lib::MTHandle<DDCLoadedData<DDCNoHeader>> compiledData = LoadDerivedData<DDCNoHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	const TerrainMaterialDerivedData& dd = *reinterpret_cast<const TerrainMaterialDerivedData*>(compiledData->bin.data());

	for (SizeType i = 0; i < dd.compiledEntries.size(); ++i)
	{
		const TerrainCompiledMaterialEntry& compiledEntry = dd.compiledEntries[i];
		const ResourcePathID materialAssetPathID = compiledEntry.materialAssetPathID;
		if (materialAssetPathID != InvalidResourcePathID)
		{
			const LoadResult<MaterialAsset> loadRes = GetOwningSystem().LoadAsset<MaterialAsset>(materialAssetPathID);
			if (!loadRes)
			{
				SPT_LOG_ERROR(TerrainMaterialAsset, "Failed to load referenced MaterialAsset for TerrainMaterialAsset '{}'. Reason: {}", GetName().ToString(), AssetLoadErrorToString(loadRes.GetError()));
				continue;
			}

			m_materialAssets.PushBack(loadRes.GetValue());
		}
	}

	for (SizeType i = 0; i < m_materialAssets.GetSize(); ++i)
	{
		m_materialAssets[i]->AwaitInitialization();
	}

	lib::StaticArray<rdr::HLSLStorage<rsc::TerrainMaterialEntry>, rsc::terrain_material_props::maxMaterialEntries> materialEntriesData{};

	for (SizeType i = 0; i < m_materialAssets.GetSize(); ++i)
	{
		const MaterialAssetHandle& materialAsset = m_materialAssets[i];
		const ecs::EntityHandle matEntity = materialAsset->GetMaterialEntity();
		const mat::MaterialProxyComponent& materialProxy = matEntity.get<mat::MaterialProxyComponent>();

		rsc::TerrainMaterialEntry materialEntry;
		materialEntry.dataHandle = materialProxy.GetMaterialDataHandle();
		materialEntry.uvScale    = dd.compiledEntries[i].uvScale;
		materialEntry.grassType  = dd.compiledEntries[i].grassType;

		materialEntriesData[i] = materialEntry;
	}

	rhi::BufferDefinition materialDataBufferDef(sizeof(materialEntriesData), rhi::EBufferUsage::Storage);
	lib::SharedRef<rdr::Buffer> materialEntries = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Material Entires Buffer"), materialDataBufferDef, rhi::EMemoryUsage::GPUOnly);
		
	rdr::GPUApi::GetTransfersManager().EnqueueUpload(materialEntries, 0u, reinterpret_cast<const Byte*>(materialEntriesData.data()), sizeof(materialEntriesData));

	m_terrainMaterialData.matEntries = materialEntries->GetFullView();
}

} // spt::as
