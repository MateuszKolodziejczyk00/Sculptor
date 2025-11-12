#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "Material.h"
#include "MaterialFactory.h"
#include "Shaders/ShaderTypes.h"
#include "Common/ShaderCompilerTypes.h"
#include "ShaderStructs/ShaderStructs.h"

#include <variant>


namespace spt::mat
{

struct MaterialDefaultShader
{
	lib::HashedString materialShaderPath;
	lib::DynamicArray<lib::HashedString> macroDefinitions;
};


struct MaterialDefaultShadersConfig
{
	lib::HashMap<lib::HashedString, MaterialDefaultShader> materialDefaultShaders;
};


class MATERIALS_API MaterialsSubsystem
{
public:

	static MaterialsSubsystem& Get();

	template<typename TMaterialData>
	ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData, ecs::EntityHandle shaderEntity);

	template<typename TMaterialData>
	ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData);

	const lib::DynamicArray<MaterialShader>&        GetMaterialShaders() const;
	const lib::DynamicArray<RTHitGroupPermutation>& GetRTHitGroups() const;
	Uint32 GetRTHitGroupIdx(const RTHitGroupPermutation& hitGroupPermutation) const;

private:

	MaterialsSubsystem();

	ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle shaderEntity, const Byte* materialData, Uint64 dataSize, lib::HashedString dataStructName);

	ecs::EntityHandle GetOrCreateShaderEntity(lib::HashedString materialDataStruct);

	lib::HashMap<lib::HashedString, ecs::EntityHandle> m_shaderSourceEntities;

	lib::UniquePtr<IMaterialFactory> m_materialFactory;

	lib::DynamicArray<MaterialShader>         m_materialShaders;
	lib::DynamicArray<RTHitGroupPermutation > m_hitGroups;
	lib::HashMap<RTHitGroupPermutation, Uint32, rdr::ShaderStructHasher<RTHitGroupPermutation>> m_hitGroupToIdx;

	MaterialDefaultShadersConfig m_defaultShadersConfig;
	lib::HashMap<lib::HashedString, ecs::EntityHandle> m_defaultShadersEntities;
};

template<typename TMaterialData>
inline ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData, ecs::EntityHandle shaderEntity)
{
	const rdr::HLSLStorage<TMaterialData> shaderMaterialData = materialData;
	return CreateMaterial(materialDef, shaderEntity, reinterpret_cast<const Byte*>(&shaderMaterialData), sizeof(rdr::HLSLStorage<TMaterialData>));
}

template<typename TMaterialData>
inline ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData)
{
	const lib::HashedString dataStructName = TMaterialData::GetStructName();
	const ecs::EntityHandle shaderEntity = m_defaultShadersEntities.at(dataStructName);
	const rdr::HLSLStorage<TMaterialData> shaderMaterialData = materialData;
	return CreateMaterial(materialDef, shaderEntity, reinterpret_cast<const Byte*>(&shaderMaterialData), sizeof(rdr::HLSLStorage<TMaterialData>), dataStructName);
}

} // spt::mat
