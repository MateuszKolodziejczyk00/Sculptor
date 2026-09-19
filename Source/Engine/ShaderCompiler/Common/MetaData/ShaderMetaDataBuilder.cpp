#include "ShaderMetaDataBuilder.h"
#include "Common/CompiledShader.h"
#include "ShaderMetaData.h"
#include "ShaderMetaDataBuilderTypes.h"


namespace spt::sc
{

namespace priv
{

static void BuildShaderMetaData(const ShaderCompilationMetaData& compilationMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	outShaderMetaData.SetShaderParamsTypes(compilationMetaData.GetShaderParamsTypes());

#if WITH_SHADERS_HOT_RELOAD
	for (const auto& [structName, versionHash] : compilationMetaData.shaderStructsVersionHashes)
	{
		outShaderMetaData.RegisterShaderStruct(structName, versionHash);
	}
#endif // WITH_SHADERS_HOT_RELOAD
}

} // priv

void ShaderMetaDataBuilder::BuildShaderMetaData(const CompiledShader& shader, const ShaderCompilationMetaData& compilationMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	priv::BuildShaderMetaData(compilationMetaData, outShaderMetaData);
}

} // spt::sc
