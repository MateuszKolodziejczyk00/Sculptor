#include "MeshPrefab.h"
#include "PrefabAsset.h"
#include "MeshAsset.h"
#include "MaterialAsset.h"
#include "AssetsSystem.h"
#include "World.h"
#include "ThreadContext.h"


SPT_DEFINE_LOG_CATEGORY(PrefabMeshEntity, true);

namespace spt::gf
{

static constexpr lib::HashedString::KeyType materialsMember = lib::HashedString::HashString("materials");


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

lib::TypeID PrefabMeshEntityDefinition::GetParameterType(const as::PrefabParameterResolveContext& context, lib::HashedString::KeyType member, Uint32 elementIdx) const
{
	return (member == materialsMember && elementIdx < materials.size()) ? lib::TypeID(lib::TypeInfo<as::MaterialAssetHandle>::id) : lib::TypeID(0u);
}

void PrefabMeshEntityDefinition::ApplyParameter(const SetParameterContext& context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload)
{
	SPT_CHECK(binding.memberKey == materialsMember);

	const MeshEntityHandle meshHandle = context.world.entities.GetTypedEntityHandle<MeshEntityHandle>(context.prefabInstance.entities, binding.entityIdx);
	const MeshEntity& mesh = context.world.meshes.entities.GetRef(meshHandle);

	context.world.materials.SetMaterial(mesh.def.materialSlots, binding.elementIdx, *static_cast<const as::MaterialAssetHandle*>(payload.data));
	context.world.meshes.entities.MarkAsDirty(meshHandle);
}

void PrefabMeshEntityDefinition::Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData)
{
	TCX_SCRATCH_SCOPE();

	const CompiledMeshEntity& compiledMesh = *reinterpret_cast<const CompiledMeshEntity*>(compiledData.data());

	as::MeshAssetHandle meshAsset = context.assetsSystem.GetLoadedAsset<as::MeshAsset>(compiledMesh.mesh);
	if (!meshAsset.IsValid())
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - mesh asset ({}) is invalid. Resolved path: {}", compiledMesh.mesh, context.assetsSystem.ResolvePath(compiledMesh.mesh).GetPath());
		return;
	}

	if (meshAsset->GetSubmeshesNum() != compiledMesh.materialsNum)
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - number of materials ({}) does not match number of submeshes ({}) in mesh asset ({})", compiledMesh.materialsNum, meshAsset->GetSubmeshesNum(), compiledMesh.mesh);
		return;
	}

	const lib::Span<const as::ResourcePathID> materialAssets(reinterpret_cast<const as::ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), compiledMesh.materialsNum);

	lib::ManagedSpan<as::MaterialAssetHandle> materialSlotsArray = tcx::Scratch().AllocateArray<as::MaterialAssetHandle>(materialAssets.size());
	for (Uint32 matIdx = 0u; matIdx < materialAssets.size(); ++matIdx)
	{
		materialSlotsArray[matIdx] = context.assetsSystem.GetLoadedAsset<as::MaterialAsset>(materialAssets[matIdx]);
	}

	const MeshEntityHandle meshHandle = context.world.meshes.entities.Add(MeshEntity
		{
			.def =
			{
				.mesh          = meshAsset,
				.materialSlots = context.world.materials.CreateMaterialSlotsChain(materialSlotsArray),
				.transform     = compiledMesh.transform,
				.owningPrefab  = &context.prefabInstance
			},
			.next = context.prefabInstance.meshes
		});
	context.prefabInstance.meshes = meshHandle;

	context.world.entities.SetTypedEntity(context.prefabInstance.entities, entityIdx, meshHandle);
}

} // spt::gf
