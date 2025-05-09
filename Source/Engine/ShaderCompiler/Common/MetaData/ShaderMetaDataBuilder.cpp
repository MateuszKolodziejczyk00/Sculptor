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
	
	for (SizeType dsIdx = 0; dsIdx < compilationMetaData.GetDescriptorSetsNum(); ++dsIdx)
	{
		const SizeType dsTypeID = compilationMetaData.GetDSTypeID(dsIdx);
		outShaderMetaData.SetDescriptorSetStateTypeID(dsIdx, dsTypeID);
	}
}

} // priv

void ShaderMetaDataBuilder::BuildShaderMetaData(const CompiledShader& shader, const ShaderCompilationMetaData& compilationMetaData, smd::ShaderMetaData& outShaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	priv::BuildShaderMetaData(compilationMetaData, outShaderMetaData);
}

} // spt::sc
