#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "ConfigUtils.h"


namespace spt::mat
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialsSubsystem ============================================================================

MaterialsSubsystem& MaterialsSubsystem::Get()
{
	static MaterialsSubsystem instance;
	return instance;
}

const lib::DynamicArray<MaterialShader>& MaterialsSubsystem::GetMaterialShaders() const
{
	return m_materialShaders;
}

const lib::DynamicArray<RTHitGroupPermutation>& MaterialsSubsystem::GetRTHitGroups() const
{
	return m_hitGroups;
}

Uint32 MaterialsSubsystem::GetRTHitGroupIdx(const RTHitGroupPermutation& hitGroupPermutation) const
{
	return m_hitGroupToIdx.at(hitGroupPermutation);
}

const lib::DynamicArray<lib::HashedString>& MaterialsSubsystem::GetMaterialDataStructNames() const
{
	return m_materialDataStructNames;
}

MaterialsSubsystem::MaterialsSubsystem()
{
	m_materialFactory = std::make_unique<MaterialFactory>();

	engn::ConfigUtils::LoadConfigData(OUT m_defaultShadersConfig, "MaterialDefaultShaders.json");

	for (const auto& [dataStruct, defaultShader] : m_defaultShadersConfig.materialDefaultShaders)
	{
		MaterialShader shader;
		shader.materialShaderHandle   = GetOrCreateShaderEntity(dataStruct);
		shader.materialDataStructName = dataStruct;

		m_materialShaders.emplace_back(shader);

		RTHitGroupPermutation rtHitGroup;
		rtHitGroup.SHADER = shader;

		m_hitGroups.emplace_back(rtHitGroup);
		m_hitGroupToIdx[rtHitGroup] = static_cast<Uint32>(m_hitGroups.size() - 1u);
	}

	m_materialDataStructNames.reserve(m_defaultShadersConfig.materialDefaultShaders.size());
	for (const auto& [dataStruct, defaultShader] : m_defaultShadersConfig.materialDefaultShaders)
	{
		m_materialDataStructNames.emplace_back(dataStruct);
	}
}

ecs::EntityHandle MaterialsSubsystem::CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle shaderEntity, const Byte* materialData, Uint64 dataSize, lib::HashedString dataStructName)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIVirtualAllocation materialDataSuballocation = MaterialsUnifiedData::Get().CreateMaterialDataSuballocation(materialData, static_cast<Uint32>(dataSize));

	MaterialDataParameters dataParameters;
	dataParameters.suballocation			= materialDataSuballocation;
	dataParameters.materialDataStructName	= dataStructName;

	const ecs::EntityHandle materialEntity = m_materialFactory->CreateMaterial(materialDef, shaderEntity, dataParameters);

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
