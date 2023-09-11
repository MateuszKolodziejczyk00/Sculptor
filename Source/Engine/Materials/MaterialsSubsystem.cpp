#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "SculptorYAML.h"
#include "ConfigUtils.h"

namespace spt::srl
{

template<>
struct TypeSerializer<mat::MaterialDefaultShader>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("MaterialShaderPath", data.materialShaderPath);
	}
};


template<>
struct TypeSerializer<mat::MaterialDefaultShadersConfig>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("MaterialDefaultShaders", data.materialDefaultShaders);
	}
};

} // spt::srl


SPT_YAML_SERIALIZATION_TEMPLATES(spt::mat::MaterialDefaultShader);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::mat::MaterialDefaultShadersConfig);


namespace spt::mat
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialShadersCache ==========================================================================

MaterialShadersCache::MaterialShadersCache()
{
	m_shadersCompiler = std::make_unique<MaterialShadersCompiler>();
	m_shadersCompiler->LoadTechniquesRegistry();
}

void MaterialShadersCache::RegisterMaterial(const MaterialProxyComponent& materialProxy)
{
	if (!m_materialParamsCache.contains(materialProxy.materialShadersHash))
	{
		m_materialParamsCache[materialProxy.materialShadersHash] = materialProxy.params;
	}
}

SizeType MaterialShadersCache::GetMaterialShadersHash(lib::HashedString techniqueName, MaterialShadersHash shaderHash, const MaterialShadersParameters& parameters) const
{
	return lib::HashCombine(techniqueName, parameters.GetHash(), shaderHash);
}

std::pair<MaterialShadersCache::CachedShaders&, Bool> MaterialShadersCache::GetOrAddCachedShaders(SizeType hash)
{
	const auto foundShaders = m_shadersCache.find(hash);
	if (foundShaders != std::cend(m_shadersCache))
	{
		return { foundShaders->second, false };
	}

	auto [it2, success] = m_shadersCache.emplace(hash, CachedShaders{});
	return { it2->second, !success };
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialsManager ==============================================================================

MaterialsSubsystem& MaterialsSubsystem::Get()
{
	static MaterialsSubsystem instance;
	return instance;
}

MaterialsSubsystem::MaterialsSubsystem()
{
	m_materialFactory = std::make_unique<MaterialFactory>();

	engn::ConfigUtils::LoadConfigData(OUT m_defaultShadersConfig, "MaterialDefaultShaders.yaml");
}

ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle shaderEntity, const Byte* materialData, Uint64 dataSize, lib::HashedString dataStructName)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISuballocation materialDataSuballocation = MaterialsUnifiedData::Get().CreateMaterialDataSuballocation(materialData, static_cast<Uint32>(dataSize));

	MaterialDataParameters dataParameters;
	dataParameters.suballocation			= materialDataSuballocation;
	dataParameters.materialDataStructName	= dataStructName;

	const ecs::EntityHandle materialEntity = m_materialFactory->CreateMaterial(materialDef, shaderEntity, dataParameters);

	m_shadersCache.RegisterMaterial(materialEntity.get<MaterialProxyComponent>());

	return materialEntity;
}

ecs::EntityHandle MaterialsSubsystem::GetOrCreateShaderEntity(lib::HashedString materialDataStruct)
{
	SPT_PROFILER_FUNCTION();

	ecs::EntityHandle& shaderEntity = m_defaultShadersEntities[materialDataStruct];

	if (!shaderEntity)
	{
		shaderEntity = ecs::CreateEntity();

		const MaterialDefaultShader& defaultShader = m_defaultShadersConfig.materialDefaultShaders.at(materialDataStruct);

		MaterialShaderSourceComponent shaderSourceComponent;
		shaderSourceComponent.shaderPath		= defaultShader.materialShaderPath;
		shaderSourceComponent.macroDefinitions	= defaultShader.macroDefinitions;

		shaderEntity.emplace<MaterialShaderSourceComponent>(std::move(shaderSourceComponent));
	}

	return shaderEntity;
}

} // spt::mat
