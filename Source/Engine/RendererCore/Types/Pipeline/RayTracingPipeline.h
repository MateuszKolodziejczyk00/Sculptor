#pragma once

#include "Pipeline.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIShaderBindingTableImpl.h"


namespace spt::rdr
{

struct RayTracingHitGroupShaders
{
	lib::SharedPtr<Shader> closestHitShader;
	lib::SharedPtr<Shader> anyHitShader;
	lib::SharedPtr<Shader> intersectionShader;
};

struct RayTracingPipelineShaderObjects
{
	RayTracingPipelineShaderObjects() = default;

	lib::SharedPtr<Shader> rayGenShader;
	lib::DynamicArray<RayTracingHitGroupShaders> hitGroups;
	lib::DynamicArray<lib::SharedRef<Shader>> missShaders;
};


class RENDERER_CORE_API RayTracingPipeline : public Pipeline
{
protected:

	using Super = Pipeline;

public:

	RayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaderObjects& shaders, const rhi::RayTracingPipelineDefinition& definition);
	~RayTracingPipeline();

	const rhi::RHIShaderBindingTable& GetShaderBindingTable() const;

private:

	rhi::RHIShaderBindingTable m_shaderBindingTable;
};

} // spt::rdr
