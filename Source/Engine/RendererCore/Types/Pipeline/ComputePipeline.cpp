#include "ComputePipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

ComputePipeline::ComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(shader->GetStage() == rhi::EShaderStage::Compute);

	AppendToPipelineMetaData(shader->GetMetaData());

	GetRHI().InitializeRHI(shader->GetRHI());
	GetRHI().SetName(name.Get());
}

} // spt::rdr
