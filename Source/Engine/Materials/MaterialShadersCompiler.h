#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"
#include "ECSRegistry.h"
#include "Shaders/ShaderTypes.h"
#include "Common/ShaderCompilerTypes.h"


namespace spt::mat
{

struct MaterialProxyComponent;
struct MaterialStaticParameters;


struct MaterialTechniqueConfig
{
	MaterialTechniqueConfig() = default;

	lib::HashedString shadersPath;

	lib::HashedString entryPointNames[static_cast<Uint32>(rhi::EShaderStage::NUM)];

	void ResolveEntryPointNames(const lib::String& techniqueName)
	{
		SPT_CHECK(!techniqueName.empty());

		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Vertex)]		= techniqueName + "_VS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::Fragment)]	= techniqueName + "_FS";
		
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::RTClosestHit)]	= techniqueName + "_RT_CHS";
		entryPointNames[static_cast<Uint32>(rhi::EShaderStage::RTAnyHit)]		= techniqueName + "_RT_AHS";
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
	rdr::ShaderID vertexShader;
	rdr::ShaderID fragmentShader;
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

	virtual sc::ShaderCompilationSettings CreateCompilationSettings(const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters) const = 0;
	virtual void LoadTechniquesRegistry(MaterialTechniquesRegistry& OUT registry) = 0;

	void CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialRayTracingShaders& rayTracingShaders);
	void CreateMaterialShadersImpl(lib::HashedString techniqueName, const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters, OUT MaterialGraphicsShaders& graphicsShaders);

	lib::HashedString GetMaterialTechniqueShadersPath(lib::HashedString techniqueName) const;
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
	virtual sc::ShaderCompilationSettings CreateCompilationSettings(const MaterialStaticParameters& materialParams, const MaterialShadersParameters& parameters) const override;
	virtual void LoadTechniquesRegistry(MaterialTechniquesRegistry& OUT registry) override;
	// End IMaterialShadersCompiler overrides
};

} // spt::mat