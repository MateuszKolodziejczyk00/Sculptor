#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class GraphicsPipeline;
class ComputePipeline;
class RayTracingPipeline;
class Pipeline;
class CommandQueue;
class RenderContext;


class PipelinePendingState
{
public:

	PipelinePendingState();
	~PipelinePendingState();

	// Graphics Pipeline ================================================
	
	void									BindGraphicsPipeline(const lib::SharedRef<GraphicsPipeline>& pipeline);
	void									UnbindGraphicsPipeline();
	const lib::SharedPtr<GraphicsPipeline>&	GetBoundGraphicsPipeline() const;

	void									FlushParamsForGraphicsPipeline(rhi::RHICommandBuffer& cmdBuffer);

	// Compute Pipeline =================================================
	
	void									BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline);
	void									UnbindComputePipeline();
	const lib::SharedPtr<ComputePipeline>&	GetBoundComputePipeline() const;

	void									FlushParamsForComputePipeline(rhi::RHICommandBuffer& cmdBuffer);

	// Ray Tracing Pipeline =============================================
	
	void										BindRayTracingPipeline(const lib::SharedRef<RayTracingPipeline>& pipeline);
	void										UnbindRayTracingPipeline();
	const lib::SharedPtr<RayTracingPipeline>&	GetBoundRayTracingPipeline() const;

	void										FlushParamsForRayTracingPipeline(rhi::RHICommandBuffer& cmdBuffer);
	
	// Shader Params ====================================================
	
	void BindShaderParams(lib::HashedString type, rhi::DeviceAddress address);
	void UnbindShaderParams(lib::HashedString type);

private:

	using ShaderParamsData = lib::InlineDynamicArray<Byte, 64u>;

	struct PendingBindings
	{
		ShaderParamsData shaderParamsData;
	};

	struct BoundShaderParam
	{
		lib::HashedString  type;
		rhi::DeviceAddress address;
	};

	PendingBindings FlushPendingParams(const lib::SharedRef<Pipeline>& pipeline);

	lib::SharedPtr<GraphicsPipeline>	m_boundGfxPipeline;
	lib::SharedPtr<ComputePipeline>		m_boundComputePipeline;
	lib::SharedPtr<RayTracingPipeline>	m_boundRayTracingPipeline;

	lib::InlineDynamicArray<BoundShaderParam, 64u> m_boundShaderParams;
};

} // spt::rdr
