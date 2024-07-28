#include "TracesAllocator.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

TracesAllocator::TracesAllocator()
{
}

void TracesAllocator::Initialize(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	SPT_PROFILER_FUNCTION();

	{
		const Uint32 tracesNum = resolution.x() * resolution.y();

		rhi::BufferDefinition bufferDef;
		bufferDef.size = tracesNum * sizeof(EncodedRayTraceCommand);;
		bufferDef.usage = rhi::EBufferUsage::Storage;
		m_rayTracesCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray Traces Commands Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
	}

	{
		rhi::BufferDefinition bufferDef;
		bufferDef.size = sizeof(math::Vector4u);
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::TransferDst);
		m_commandsNum = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray Tracing Commands Num Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Trace Commands Num"), m_commandsNum, 0u);
	}

	m_descriptorSet = graphBuilder.CreateDescriptorSet<TracesAllocatorDS>(RENDERER_RESOURCE_NAME("TracesAllocatorDS"));
	m_descriptorSet->u_rayTracesCommands      = m_rayTracesCommands;
	m_descriptorSet->u_commandsNum            = m_commandsNum;
}

} // spt::rsc
