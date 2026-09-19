#include "Pipeline.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipeline ======================================================================================

Pipeline::Pipeline()
{ }

const smd::ShaderMetaData& Pipeline::GetMetaData() const
{
	return m_metaData;
}

void Pipeline::AppendToPipelineMetaData(const smd::ShaderMetaData& shaderMetaData)
{
	SPT_PROFILER_FUNCTION();

	m_metaData.Append(shaderMetaData);
}

} // spt::rdr
