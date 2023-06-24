#include "ComputePipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

ComputePipeline::ComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(shader->GetStage() == rhi::EShaderStage::Compute);

	AppendToPipelineMetaData(shader->GetMetaData());

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition();

	GetRHI().InitializeRHI(shader->GetRHI(), pipelineLayoutDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
