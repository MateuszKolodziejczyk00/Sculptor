#include "ComputePipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

ComputePipeline::ComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
	: Pipeline(shader)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<rhi::RHIShaderModule>& shaderModules = shader->GetShaderModules();
	SPT_CHECK(shaderModules.size() == 1); // compute shader must have only one module (compute)

	const rhi::RHIShaderModule& computeShaderModule = shaderModules[0];
	SPT_CHECK(computeShaderModule.GetStage() == rhi::EShaderStage::Compute);

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition(*GetMetaData());

	GetRHI().InitializeRHI(computeShaderModule, pipelineLayoutDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
