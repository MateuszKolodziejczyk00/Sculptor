#include "ShaderMetaDataBuilder.h"
#include "Common/CompiledShader.h"
#include "ShaderMetaDataBuilderTypes.h"
#include "ShaderMetaData.h"


namespace spt::sc
{

namespace priv
{

static void BuildShaderMetaData(const ShaderCompilationMetaData& compilationMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_MSG(compilationMetaData.GetDSTypeID(0u) == idxNone<Uint64>, "Shaders using bindless cannot use descriptor set 0");
	
	for (SizeType dsIdx = 0; dsIdx < compilationMetaData.GetDescriptorSetsNum(); ++dsIdx)
	{
		const SizeType dsTypeID = compilationMetaData.GetDSTypeID(dsIdx);
		outShaderMetaData.SetDescriptorSetStateTypeID(dsIdx, dsTypeID);
	}

	outShaderMetaData.SetShaderParamsTypeName(compilationMetaData.GetShaderParamsTypeName());

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
