#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "Material.h"
#include "MaterialFactory.h"
#include "Shaders/ShaderTypes.h"
#include "Common/ShaderCompilerTypes.h"
#include "MaterialShadersCompiler.h"

#include <variant>


namespace spt::mat
{

class MATERIALS_API MaterialShadersCache
{
public:

	MaterialShadersCache();

	void RegisterMaterial(const MaterialProxyComponent& materialProxy);

	template<typename TMaterialShaders>
	TMaterialShaders GetMaterialShaders(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters);
	
private:

	using CachedShaders = std::variant<MaterialRayTracingShaders, MaterialGraphicsShaders>;

	SizeType GetMaterialShadersHash(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters) const;

	std::pair<CachedShaders&, Bool> GetOrAddCachedShaders(SizeType hash);

	MaterialStaticParameters GetMaterialStaticParameters(MaterialShadersHash shaderHash) const;

	lib::UniquePtr<IMaterialShadersCompiler> m_shadersCompiler;
	
	lib::HashMap<SizeType, CachedShaders> m_shadersCache;

	lib::HashMap<MaterialShadersHash, MaterialStaticParameters> m_materialParamsCache;
};

template<typename TMaterialShaders>
TMaterialShaders MaterialShadersCache::GetMaterialShaders(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters)
{
	const SizeType hash = GetMaterialShadersHash(techniqueName, shaderHash, parameters);

	const auto [cachedShaders, foundExisting] = GetOrAddCachedShaders(hash);

	if (!foundExisting)
	{
		cachedShaders = TMaterialShaders{};
		const MaterialStaticParameters materialParams = GetMaterialStaticParameters(shaderHash);
		m_shadersCompiler->CreateMaterialShaders(techniqueName, materialParams, parameters, OUT std::get<TMaterialShaders>(cachedShaders));
	}

	return std::get<TMaterialShaders>(cachedShaders);
}


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

	template<typename TMaterialShaders>
	TMaterialShaders GetMaterialShaders(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters = MaterialShadersParameters());


private:

	MaterialsSubsystem();

	ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle shaderEntity, const Byte* materialData, Uint64 dataSize, lib::HashedString dataStructName);

	ecs::EntityHandle GetOrCreateShaderEntity(lib::HashedString materialDataStruct);

	lib::HashMap<lib::HashedString, ecs::EntityHandle> m_shaderSourceEntities;

	lib::UniquePtr<IMaterialFactory> m_materialFactory;

	MaterialShadersCache m_shadersCache;

	MaterialDefaultShadersConfig m_defaultShadersConfig;
	lib::HashMap<lib::HashedString, ecs::EntityHandle> m_defaultShadersEntities;
};

template<typename TMaterialData>
inline ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData, ecs::EntityHandle shaderEntity)
{
	return CreateMaterial(materialDef, shaderEntity, reinterpret_cast<const Byte*>(&materialData), sizeof(TMaterialData));
}

template<typename TMaterialData>
inline ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, const TMaterialData& materialData)
{
	const lib::HashedString dataStructName = TMaterialData::GetStructName();
	const ecs::EntityHandle shaderEntity = GetOrCreateShaderEntity(dataStructName);
	return CreateMaterial(materialDef, shaderEntity, reinterpret_cast<const Byte*>(&materialData), sizeof(TMaterialData), dataStructName);
}

template<typename TMaterialShaders>
TMaterialShaders MaterialsSubsystem::GetMaterialShaders(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters /*= MaterialShadersParameters()*/)
{
	SPT_PROFILER_FUNCTION();

	return m_shadersCache.GetMaterialShaders<TMaterialShaders>(techniqueName, shaderHash, parameters);
}

} // spt::mat
