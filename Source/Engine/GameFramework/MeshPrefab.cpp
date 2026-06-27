#include "MeshPrefab.h"
#include "PrefabAsset.h"
#include "MeshAsset.h"
#include "MaterialAsset.h"
#include "AssetsSystem.h"
#include "World.h"


SPT_DEFINE_LOG_CATEGORY(PrefabMeshEntity, true);

namespace spt::gf
{

struct CompiledMeshEntity
{
	Transform transform;

	as::ResourcePathID mesh = as::InvalidResourcePathID;
	Uint32 materialsNum = 0u;
};


lib::DynamicArray<Byte> PrefabMeshEntityDefinition::Compile(as::PrefabCompiler& compiler) const
{
	const Uint32 materialsNum = static_cast<Uint32>(materials.size());
	const Uint32 materialsDataSize = materialsNum * sizeof(as::ResourcePathID);

	CompiledMeshEntity compiledEntity;
	compiledEntity.transform.location = location;
	compiledEntity.transform.rotation = rotation;
	compiledEntity.transform.scale    = scale;

	compiledEntity.mesh = as::prefab_compiler_api::CreateAssetDependency(compiler, mesh);

	compiledEntity.materialsNum = materialsNum;

	const Uint32 compiledDataSize = sizeof(CompiledMeshEntity) + materialsDataSize;
	lib::DynamicArray<Byte> compiledData(compiledDataSize);
	std::memcpy(compiledData.data(), &compiledEntity, sizeof(CompiledMeshEntity));

	lib::Span<as::ResourcePathID> outMaterialIDs(reinterpret_cast<as::ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), materialsNum);
	for(Uint32 matIdx = 0u; matIdx < materialsNum; ++matIdx)
	{
		outMaterialIDs[matIdx] = as::prefab_compiler_api::CreateAssetDependency(compiler, materials[matIdx]);
	}

	return compiledData;
}

void PrefabMeshEntityDefinition::Spawn(const SpawnContext& context, lib::Span<const Byte> compiledData)
{
	const CompiledMeshEntity& compiledMesh = *reinterpret_cast<const CompiledMeshEntity*>(compiledData.data());

	as::MeshAssetHandle meshAsset = context.assetsSystem.GetLoadedAsset<as::MeshAsset>(compiledMesh.mesh);
	if (!meshAsset.IsValid())
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - mesh asset ({}) is invalid. Resolved path: {}", compiledMesh.mesh, context.assetsSystem.ResolvePath(compiledMesh.mesh).GetPath().string());
		return;
	}

	if (meshAsset->GetSubmeshesNum() != compiledMesh.materialsNum)
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - number of materials ({}) does not match number of submeshes ({}) in mesh asset ({})", compiledMesh.materialsNum, meshAsset->GetSubmeshesNum(), compiledMesh.mesh);
		return;
	}

	const lib::Span<const as::ResourcePathID> materialAssets(reinterpret_cast<const as::ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), compiledMesh.materialsNum);

	lib::InlineDynamicArray<as::MaterialAssetHandle, 256u> materialSlotsArray;
	for (Uint32 matIdx = 0u; matIdx < materialAssets.size(); ++matIdx)
	{
		const as::MaterialAssetHandle material = context.assetsSystem.GetLoadedAsset<as::MaterialAsset>(materialAssets[matIdx]);
		materialSlotsArray.EmplaceBack(material);
	}

	SPT_STATIC_CHECK(MeshesChunk::MaxMeshesNum == 1u);
	MeshesChunkHandle prevChunk = context.prefabInstance.meshesChunk;
	MeshesChunk newChunk;
	newChunk.meshes[0] = MeshEntity
	{
		.def =
		{
			.mesh          = meshAsset,
			.materialSlots = context.world.materials.CreateMaterialSlotsChain(materialSlotsArray),
			.transform     = compiledMesh.transform,
			.owningPrefab  = &context.prefabInstance
		}
	};
	newChunk.next = prevChunk;
	
	context.prefabInstance.meshesChunk = context.world.meshes.chunks.Add(newChunk);
}

} // spt::gf
