
#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "VariableRateTexture.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(EncodedRayTraceCommand)
	/*
		Stores small block 4x4 offset, local offset of trace inside of this block and tracing rate
		This allows to have custom offset within each "lower rate" block for each trace
		Layout:
		[0-11]  block offset X
		[12-23] block offset Y
		[24-25] local offset X
		[26-27] local offset Y
		[28-31] tracing rate
	*/
	SHADER_STRUCT_FIELD(Uint32, coordsAndRate)
END_SHADER_STRUCT();


DS_BEGIN(TracesAllocatorDS, rg::RGDescriptorSetState<TracesAllocatorDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<EncodedRayTraceCommand>), u_rayTracesCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                 u_commandsNum)
DS_END();


struct TracesAllocation
{
	rg::RGBufferViewHandle m_rayTracesCommands;
	rg::RGBufferViewHandle m_commandsNum;
};


class TracesAllocator
{
public:

	static inline math::Vector2u s_groupSize = math::Vector2u(16u, 16u);

	TracesAllocator();

	void Initialize(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution);

	TracesAllocation GetTracesAllocation() const { return TracesAllocation{ m_rayTracesCommands, m_commandsNum }; }

	lib::MTHandle<TracesAllocatorDS> GetDescriptorSet() const { return m_descriptorSet; }

private:

	rg::RGBufferViewHandle m_rayTracesCommands;
	rg::RGBufferViewHandle m_commandsNum;

	lib::MTHandle<TracesAllocatorDS> m_descriptorSet;
};

} // spt::rsc
