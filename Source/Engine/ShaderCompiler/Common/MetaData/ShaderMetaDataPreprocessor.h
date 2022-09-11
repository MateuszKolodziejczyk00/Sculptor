
#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataBuilderTypes.h"

namespace spt::sc
{

class ShaderSourceCode;


class ShaderMetaDataPrerpocessor
{
public:

	SPT_NODISCARD static ShaderParametersMetaData PreprocessShaderParametersMetaData(ShaderSourceCode& sourceCode);
};

} // spt::sc
