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
	virtual lib::TypeID             GetParameterType(const as::PrefabParameterResolveContext& context, lib::HashedString::KeyType member, Uint32 elementIdx) const override;

	static void Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData);
	static void ApplyParameter(const SetParameterContext& context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload);

	virtual void Serialize(srl::Serializer& serializer) override
	{
		PrefabEntityDefinition::Serialize(serializer);

		serializer.Serialize("Mesh",      mesh);
		serializer.Serialize("Materials", materials);
	}
};
SPT_REGISTER_PREFAB_ENTITY_TYPE(PrefabMeshEntityDefinition);


using MeshEntityHandle = lib::PagedGenerationalPoolHandle<struct MeshEntity>;


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

	MeshEntityHandle next;
};


struct WorldMeshes
{
	WorldMeshes()
		: entities("World_MeshEntitiesPool")
	{
	}

	lib::PagedGenerationalPool<MeshEntity> entities;
};


} // spt::gf
