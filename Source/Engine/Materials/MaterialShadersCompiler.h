#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"
#include "ECSRegistry.h"
#include "Shaders/ShaderTypes.h"
#include "Common/ShaderCompilerTypes.h"
#include "Pipelines/PipelineState.h"


namespace spt::mat
{

struct MaterialProxyComponent;
struct MaterialStaticParameters;


struct MaterialTechniqueConfig
{
	MaterialTechniqueConfig()
		: allowsNoMaterial(false)
		, useMeshShadersPipeline(false)
		, useTaskShader(false)
		, rayTracingWithClosestHit(true)
	{ }

	lib::HashedString shadersPath;

	// If true, this technique can be used with without any material (e.g. for shadow pass, we can render all opaque objects with a single shader - without material)
	Bool allowsNoMaterial;

	// If true, this technique uses mesh shaders pipeline, otherwise it uses traditional vertex/fragment shaders pipeline
	Bool useMeshShadersPipeline;

	// If true, this technique uses task shader (useMeshShadersPipeline must be true)
	Bool useTaskShader;

	Bool rayTracingWithClosestHit;

	lib::HashedString entryPointNames[static_cast<Uint32>(rhi::EShaderStage::NUM)];

	void ResolveEntryPointNames(const lib::String& techniqueName)
	{
		SPT_CHECK(!techniqueName.empty());

		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Task)]     = techniqueName + "_TS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Mesh)]     = techniqueName + "_MS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Vertex)]   = techniqueName + "_VS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Fragment)] = techniqueName + "_FS";
		
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::RTClosestHit)] = techniqueName + "_RT_CHS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::RTAnyHit)]     = techniqueName + "_RT_AHS";
	}
};


struct MaterialTechniquesRegistry
{
	lib::HashMap<lib::HashedString, MaterialTechniqueConfig> techniques;
};


struct MaterialShadersParameters
{
	MaterialShadersParameters() = default;

	lib::DynamicArray<sc::MacroDefinition> macroDefinitions;

	SizeType GetHash() const
	{
		return lib::HashRange(std::cbegin(macroDefinitions), std::cend(macroDefinitions),
							  [](const sc::MacroDefinition& macroDef)
							  {
								  return macroDef.macro.GetKey();
							  });
	}
};


struct MaterialRayTracingShaders
{
	rdr::ShaderID closestHitShader;
	rdr::ShaderID anyHitShader;
};


struct MaterialGraphicsShaders
{
	rdr::ShaderID taskShader;
	rdr::ShaderID meshShader;
	rdr::ShaderID vertexShader;
	rdr::ShaderID fragmentShader;

	rdr::GraphicsPipelineShaders GetGraphicsPipelineShaders() const
	{
		rdr::GraphicsPipelineShaders shaders;
		shaders.taskShader    = taskShader;
		shaders.meshShader    = meshShader;
		shaders.vertexShader  = vertexShader;
		shaders.fragmentShader = fragmentShader;
		return shaders;
	}
};


class MATERIALS_API IMaterialShadersCompiler
{
public:

	IMaterialShadersCompiler();
	virtual ~IMaterialShadersCompiler() = default;

	void LoadTechniquesRegistry();

	template<typename TMaterialShaders>
	void CreateMaterialShaders(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT TMaterialShaders& shaders)
	{
		SPT_PROFILER_FUNCTION();

		CreateMaterialShadersImpl(techniqueName, materialParams, parameters, OUT shaders);
	}

protected:

	virtual sc::ShaderCompilationSettings CreateCompilationSettings(const MaterialTechniqueConfig& techniqueConfig, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters) const = 0;
	virtual void LoadTechniquesRegistry(MaterialTechniquesRegistry& OUT registry) = 0;

	void CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialRayTracingShaders& rayTracingShaders);
	void CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialGraphicsShaders& graphicsShaders);

	const MaterialTechniqueConfig& GetMaterialTechniqueConfig(lib::HashedString techniqueName) const;
	sc::ShaderStageCompilationDef GetShaderStageCompilationDef(lib::HashedString techniqueName, rhi::EShaderStage stage) const;

private:

	MaterialTechniquesRegistry m_techniquesRegistry;
};


class MATERIALS_API MaterialShadersCompiler : public IMaterialShadersCompiler
{
public:

	MaterialShadersCompiler() = default;

protected:

	// Begin IMaterialShadersCompiler overrides
	virtual sc::ShaderCompilationSettings CreateCompilationSettings(const MaterialTechniqueConfig& techniqueConfig, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters) const override;
	virtual void LoadTechniquesRegistry(MaterialTechniquesRegistry& OUT registry) override;
	// End IMaterialShadersCompiler overrides

private:

	sc::ShaderCompilationSettings CreateNoMaterialCompilationSettings() const;
	sc::ShaderCompilationSettings CreateMaterialCompilationSettings(const MaterialStaticParameters& materialParams) const;
};

} // spt::mat