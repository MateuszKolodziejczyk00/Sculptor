#pragma once

#include "PrefabEntity.h"


namespace spt::as
{

struct PrefabMeshEntity : public PrefabEntity
{
	lib::Path                    mesh;
	lib::DynamicArray<lib::Path> materials;

	virtual lib::RuntimeTypeInfo    GetType() const override { return lib::TypeInfo<PrefabMeshEntity>(); }
	virtual lib::DynamicArray<Byte> Compile(PrefabCompiler& compiler) const override;

	static void Spawn(const PrefabSpawningContext& context, lib::Span<const Byte> compiledData);

	virtual void Serialize(srl::Serializer& serializer) override
	{
		PrefabEntity::Serialize(serializer);

		serializer.Serialize("Mesh",      mesh);
		serializer.Serialize("Materials", materials);
	}
};
SPT_REGISTER_PREFAB_ENTITY_TYPE(PrefabMeshEntity);

} // spt::as
