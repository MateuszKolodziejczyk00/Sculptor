#include "MaterialShadersCompiler.h"
#include "ResourcesManager.h"
#include "Material.h"
#include "SculptorYAML.h"
#include "ConfigUtils.h"

namespace spt::srl
{

template<>
struct TypeSerializer<mat::MaterialTechniqueConfig>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("ShadersPath", data.shadersPath);
		serializer.Serialize("AllowsNoMaterial", data.allowsNoMaterial);
		serializer.Serialize("UseMeshShadersPipeline", data.useMeshShadersPipeline);
		serializer.Serialize("UseTaskShader", data.useTaskShader);
		serializer.Serialize("RayTracingWithClosestHit", data.rayTracingWithClosestHit);
	}
};


template<>
struct TypeSerializer<mat::MaterialTechniquesRegistry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Techniques", data.techniques);

		if constexpr (Serializer::IsLoading())
		{
			for (auto& [name, techniqueDef] : data.techniques)
			{
				techniqueDef.ResolveEntryPointNames(name.ToString());
			}
		}
	}
};

} // spt::srl


SPT_YAML_SERIALIZATION_TEMPLATES(spt::mat::MaterialTechniqueConfig);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::mat::MaterialTechniquesRegistry);


namespace spt::mat
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// IMaterialShadersCompiler ======================================================================

IMaterialShadersCompiler::IMaterialShadersCompiler()
{ }

void IMaterialShadersCompiler::LoadTechniquesRegistry()
{
	LoadTechniquesRegistry(OUT m_techniquesRegistry);
}

void IMaterialShadersCompiler::CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialRayTracingShaders& rayTracingShaders)
{
	const MaterialTechniqueConfig& techniqueConfig = GetMaterialTechniqueConfig(techniqueName);

	const sc::ShaderCompilationSettings compilationSettings = CreateCompilationSettings(techniqueConfig, materialParams, parameters);
	
	const lib::String shadersPath = techniqueConfig.shadersPath.ToString();

	if (techniqueConfig.rayTracingWithClosestHit)
	{
		rayTracingShaders.closestHitShader = rdr::ResourcesManager::CreateShader(shadersPath,
																				 GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::RTClosestHit),
																				 compilationSettings);
	}

	const Bool hasAnyHitShader = materialParams.customOpacity;

	if (hasAnyHitShader)
	{
		rayTracingShaders.anyHitShader = rdr::ResourcesManager::CreateShader(shadersPath,
																			 GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::RTAnyHit),
																			 compilationSettings);
	}
}

void IMaterialShadersCompiler::CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialGraphicsShaders& graphicsShaders)
{
	SPT_PROFILER_FUNCTION();

	const MaterialTechniqueConfig& techniqueConfig = GetMaterialTechniqueConfig(techniqueName);

	const sc::ShaderCompilationSettings compilationSettings = CreateCompilationSettings(techniqueConfig, materialParams, parameters);
	
	const lib::String shadersPath = techniqueConfig.shadersPath.ToString();

	if (techniqueConfig.useMeshShadersPipeline)
	{
		if (techniqueConfig.useTaskShader)
		{
			graphicsShaders.taskShader = rdr::ResourcesManager::CreateShader(shadersPath,
																			 GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::Task),
																			 compilationSettings);
		}

		graphicsShaders.meshShader = rdr::ResourcesManager::CreateShader(shadersPath,
																		GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::Mesh),
																		compilationSettings);
	}
	else
	{
		graphicsShaders.vertexShader = rdr::ResourcesManager::CreateShader(shadersPath,
																		   GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::Vertex),
																		   compilationSettings);
	}

	graphicsShaders.fragmentShader = rdr::ResourcesManager::CreateShader(shadersPath,
																		 GetShaderStageCompilationDef(techniqueName, rhi::EShaderStage::Fragment),
																		 compilationSettings);
}

const MaterialTechniqueConfig& IMaterialShadersCompiler::GetMaterialTechniqueConfig(lib::HashedString techniqueName) const
{
	const auto it = m_techniquesRegistry.techniques.find(techniqueName);
	SPT_CHECK_MSG(it != m_techniquesRegistry.techniques.end(), "Material technique \"{}\" not found", techniqueName.ToString().c_str());

	return it->second;
}

sc::ShaderStageCompilationDef IMaterialShadersCompiler::GetShaderStageCompilationDef(lib::HashedString techniqueName, rhi::EShaderStage stage) const
{
	const auto it = m_techniquesRegistry.techniques.find(techniqueName);
	SPT_CHECK_MSG(it != m_techniquesRegistry.techniques.end(), "Material technique \"{}\" not found", techniqueName.ToString().c_str());
	
	const lib::HashedString entryPoint = it->second.entryPointNames[static_cast<Uint32>(stage)];

	return sc::ShaderStageCompilationDef(stage, entryPoint);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialShadersCompiler =======================================================================

sc::ShaderCompilationSettings MaterialShadersCompiler::CreateCompilationSettings(const MaterialTechniqueConfig& techniqueConfig, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters) const
{
	sc::ShaderCompilationSettings compilationSettings;

	if (materialParams.IsValidMaterial())
	{
		compilationSettings = CreateMaterialCompilationSettings(materialParams);
	}
	else
	{
		SPT_CHECK(techniqueConfig.allowsNoMaterial);
		compilationSettings = CreateNoMaterialCompilationSettings();
	}

	for (const sc::MacroDefinition& macroDef : parameters.macroDefinitions)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition(macroDef));
	}

	return compilationSettings;
}

void MaterialShadersCompiler::LoadTechniquesRegistry(MaterialTechniquesRegistry& OUT registry)
{
	engn::ConfigUtils::LoadConfigData(OUT registry, "MaterialTechniquesRegistry.yaml");
}

sc::ShaderCompilationSettings MaterialShadersCompiler::CreateNoMaterialCompilationSettings() const
{
	sc::ShaderCompilationSettings compilationSettings;

	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_ENABLED", "0"));

	return compilationSettings;
}

sc::ShaderCompilationSettings MaterialShadersCompiler::CreateMaterialCompilationSettings(const MaterialStaticParameters& materialParams) const
{
	const MaterialShaderSourceComponent& shaderSourceComp = materialParams.materialShaderHandle.get<MaterialShaderSourceComponent>();

	sc::ShaderCompilationSettings compilationSettings;
	
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_ENABLED", "1"));

	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_SHADER_PATH", "\"" + shaderSourceComp.shaderPath.ToString() + "\""));

	for (const sc::MacroDefinition& macroDef : shaderSourceComp.macroDefinitions)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition(macroDef));
	}

	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_DATA_TYPE", materialParams.materialDataStructName));

	if (materialParams.customOpacity)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_CUSTOM_OPACITY"));
	}

	if (materialParams.doubleSided)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_DOUBLE_SIDED"));
	}

	return compilationSettings;
}

} // spt::mat
