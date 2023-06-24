#include "Shader.h"
#include "RendererUtils.h"

namespace spt::rdr
{

Shader::Shader(const RendererResourceName& name, const rhi::ShaderModuleDefinition& moduleDef, smd::ShaderMetaData&& metaData)
	: m_metaData(metaData)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(moduleDef);
	GetRHI().SetName(name.Get());
	
	m_pipelineType = rhi::GetPipelineTypeForShaderStage(moduleDef.stage);
}

const smd::ShaderMetaData& Shader::GetMetaData() const
{
	return m_metaData;
}

rhi::EPipelineType Shader::GetPipelineType() const
{
	return m_pipelineType;
}

rhi::EShaderStage Shader::GetStage() const
{
	return GetRHI().GetStage();
}

} // spt::rdr

