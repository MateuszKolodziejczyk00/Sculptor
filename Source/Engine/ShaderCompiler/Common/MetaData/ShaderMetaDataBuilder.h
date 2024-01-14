#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaData.h"


namespace spt::smd
{
class ShaderMetaData;
} // spt::smt

namespace spt::sc
{

class CompiledShader;
struct ShaderCompilationMetaData;

class ShaderMetaDataBuilder
{
public:

	static void BuildShaderMetaData(const CompiledShader& shader, const ShaderCompilationMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData);
	static void FinishBuildingMetaData(smd::ShaderMetaData& outShaderMetaData);
};

} // spt:sc
