
#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataBuilderTypes.h"

namespace spt::sc
{

class ShaderSourceCode;


class ShaderMetaDataPrerpocessor
{
public:

	SPT_NODISCARD static ShaderParametersMetaData PreprocessShader(ShaderSourceCode& sourceCode);

private:

	static void PreprocessShaderStructs(ShaderSourceCode& sourceCode, ShaderParametersMetaData& outMetaData);
	static void PreprocessShaderDescriptorSets(ShaderSourceCode& sourceCode, ShaderParametersMetaData& outMetaData);
	static void PreprocessShaderParametersMetaData(ShaderSourceCode& sourceCode, ShaderParametersMetaData& outMetaData);
};

} // spt::sc
