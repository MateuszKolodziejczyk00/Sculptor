#pragma once

#include "Entities/PrefabEntity.h"
#include "GamePrefab.h"
#include "MeshAsset.h"
#include "WorldMaterials.h"
#include "RenderSceneTypes.h"
#include "StaticMeshes/RetainedDraws.h"
#include "RayTracing/RTScene.h"


namespace spt::gf
{

struct PrefabMeshEntityDefinition : public GamePrefabEntityDefinition
{
	lib::Path                    mesh;
	lib::DynamicArray<lib::Path> materials;

	virtual lib::RuntimeTypeInfo    GetType() const override { return lib::TypeInfo<PrefabMeshEntityDefinition>(); }
	virtual lib::DynamicArray<Byte> Compile(as::PrefabCompiler& compiler) const override;

	static void Spawn(const SpawnContext& context, lib::Span<const Byte> compiledData);

	virtual void Serialize(srl::Serializer& serializer) override
	{
		PrefabEntityDefinition::Serialize(serializer);

		serializer.Serialize("Mesh",      mesh);
		serializer.Serialize("Materials", materials);
	}
};
SPT_REGISTER_PREFAB_ENTITY_TYPE(PrefabMeshEntityDefinition);

struct MeshEntity
{
	struct
	{
		as::MeshAssetHandle           mesh;
		MaterialAssetSlotsChunkHandle materialSlots;
		Transform                     transform;
		PrefabInstance*               owningPrefab = nullptr;
	} def;

	struct
	{
		rsc::RenderInstanceHandle     instance;
		rsc::MaterialSlotsChunkHandle renderMaterialSlots;
		rsc::RetainedDrawHandle       draw;
		rsc::RTInstanceHandle         rtInstance;
	} render;
};


using MeshesChunkHandle = lib::PagedGenerationalPoolHandle<struct MeshesChunk>;


struct MeshesChunk
{
	static constexpr Uint32 MaxMeshesNum = 1u;
	lib::StaticArray<MeshEntity, MaxMeshesNum> meshes;
	MeshesChunkHandle next;
};


struct WorldMeshes
{
	WorldMeshes()
		: chunks("World_MeshesChunksPool")
	{
	}

	lib::PagedGenerationalPool<MeshesChunk> chunks;
};


} // spt::gf
