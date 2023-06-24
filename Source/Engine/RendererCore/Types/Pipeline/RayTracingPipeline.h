#pragma once

#include "Pipeline.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIShaderBindingTableImpl.h"


namespace spt::rdr
{

class RENDERER_CORE_API RayTracingPipeline : public Pipeline
{
protected:

	using Super = Pipeline;

public:

	RayTracingPipeline(const RendererResourceName& name, const lib::DynamicArray<lib::SharedRef<Shader>>& shaders, const rhi::RayTracingPipelineDefinition& definition);
	~RayTracingPipeline();

	const rhi::RHIShaderBindingTable& GetShaderBindingTable() const;

private:

	rhi::RHIShaderBindingTable m_shaderBindingTable;
};

} // spt::rdr
