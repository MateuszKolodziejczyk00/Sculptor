#include "MaterialShader.h"
#include "Material.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::mat
{

void FillMaterialShaderCompilationSettings(const mat::MaterialShader& permutationDef, sc::ShaderCompilationSettings& outSettings)
{
	if (permutationDef.IsValidMaterial())
	{
		const MaterialShaderSourceComponent& shaderSourceComp = permutationDef.materialShaderHandle.get<MaterialShaderSourceComponent>();

		outSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_ENABLED", "1"));

		outSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_SHADER_PATH", "\"" + shaderSourceComp.shaderPath.ToString() + "\""));

		for (const sc::MacroDefinition& macroDef : shaderSourceComp.macroDefinitions)
		{
			outSettings.AddMacroDefinition(sc::MacroDefinition(macroDef));
		}

		outSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_DATA_TYPE", permutationDef.materialDataStructName));
	}
	else
	{
		outSettings.AddMacroDefinition(sc::MacroDefinition("SPT_MATERIAL_ENABLED", "0"));
	}
}

} // spt::mat

