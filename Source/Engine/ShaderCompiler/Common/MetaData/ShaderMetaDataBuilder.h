#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaData.h"


namespace spt::smd
{
class ShaderMetaData;
} // spt::smt

namespace spt::sc
{

struct CompiledShader;
struct ShaderParametersMetaData;

class ShaderMetaDataBuilder
{
public:

	static void BuildShaderMetaData(const CompiledShader& shader, const ShaderParametersMetaData& parametersMetaData, smd::ShaderMetaData& outShaderMetaData);
};

} // spt:sc