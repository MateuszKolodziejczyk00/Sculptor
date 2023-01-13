
#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataBuilderTypes.h"

namespace spt::sc
{

class ShaderMetaDataPrerpocessor
{
public:

	SPT_NODISCARD static ShaderParametersMetaData PreprocessShader(lib::String& sourceCode);

private:

	static void PreprocessShaderStructs(lib::String& sourceCode, ShaderParametersMetaData& outMetaData);
	static void PreprocessShaderDescriptorSets(lib::String& sourceCode, ShaderParametersMetaData& outMetaData);
	static void PreprocessShaderParametersMetaData(lib::String& sourceCode, ShaderParametersMetaData& outMetaData);
};

} // spt::sc
