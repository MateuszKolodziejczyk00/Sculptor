#include "PipelinePendingState.h"
#include "GPUApi.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Pipeline/RayTracingPipeline.h"
#include "Types/CommandBuffer.h"
#include "Types/RenderContext.h"
#include "ShaderMetaData.h"

namespace spt::rdr
{

SPT_DEFINE_LOG_CATEGORY(PipelinePendingState, false);

PipelinePendingState::PipelinePendingState()
{
}

PipelinePendingState::~PipelinePendingState() = default;

void PipelinePendingState::BindGraphicsPipeline(const lib::SharedRef<GraphicsPipeline>& pipeline)
{
	if (m_boundGfxPipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<GraphicsPipeline> prevPipeline = std::move(m_boundGfxPipeline);
		
	m_boundGfxPipeline = pipeline.ToSharedPtr();
}

void PipelinePendingState::UnbindGraphicsPipeline()
{
	m_boundGfxPipeline.reset();
}

const lib::SharedPtr<GraphicsPipeline>& PipelinePendingState::GetBoundGraphicsPipeline() const
{
	return m_boundGfxPipeline;
}

void PipelinePendingState::FlushParamsForGraphicsPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundGfxPipeline);

	const PendingBindings pendingBindings = FlushPendingParams(lib::Ref(m_boundGfxPipeline));

	if (!pendingBindings.shaderParamsData.IsEmpty())
	{
		cmdBuffer.PushData(pendingBindings.shaderParamsData);
	}
}

void PipelinePendingState::BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline)
{
	if (m_boundComputePipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<ComputePipeline> prevPipeline = std::move(m_boundComputePipeline);
		
	m_boundComputePipeline = pipeline.ToSharedPtr();
}

void PipelinePendingState::UnbindComputePipeline()
{
	m_boundComputePipeline.reset();
}

const lib::SharedPtr<ComputePipeline>& PipelinePendingState::GetBoundComputePipeline() const
{
	return m_boundComputePipeline;
}

void PipelinePendingState::FlushParamsForComputePipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundComputePipeline);

	const PendingBindings pendingBindings = FlushPendingParams(lib::Ref(m_boundComputePipeline));

	if (!pendingBindings.shaderParamsData.IsEmpty())
	{
		cmdBuffer.PushData(pendingBindings.shaderParamsData);
	}
}

void PipelinePendingState::BindRayTracingPipeline(const lib::SharedRef<RayTracingPipeline>& pipeline)
{
	if (m_boundRayTracingPipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<RayTracingPipeline> prevPipeline = std::move(m_boundRayTracingPipeline);
		
	m_boundRayTracingPipeline = pipeline.ToSharedPtr();
}

void PipelinePendingState::UnbindRayTracingPipeline()
{
	m_boundRayTracingPipeline.reset();
}

const lib::SharedPtr<rdr::RayTracingPipeline>& PipelinePendingState::GetBoundRayTracingPipeline() const
{
	return m_boundRayTracingPipeline;
}

void PipelinePendingState::FlushParamsForRayTracingPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundRayTracingPipeline);

	const PendingBindings pendingBindings = FlushPendingParams(lib::Ref(m_boundRayTracingPipeline));

	if (!pendingBindings.shaderParamsData.IsEmpty())
	{
		cmdBuffer.PushData(pendingBindings.shaderParamsData);
	}
}

void PipelinePendingState::BindShaderParams(lib::HashedString type, rhi::DeviceAddress address)
{
	m_boundShaderParams.EmplaceBack(BoundShaderParam{ type, address });
}

void PipelinePendingState::UnbindShaderParams(lib::HashedString type)
{
	for (SizeType i = m_boundShaderParams.size(); i > 0; --i)
	{
		if (m_boundShaderParams[i - 1].type == type)
		{
			m_boundShaderParams.RemoveAt(i - 1);
			break;
		}
	}
}

PipelinePendingState::PendingBindings PipelinePendingState::FlushPendingParams(const lib::SharedRef<Pipeline>& pipeline)
{
	const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

	PendingBindings pendingBindings;

	const lib::DynamicArray<lib::HashedString>& shaderParamsTypes = metaData.GetShaderParamsTypes();

	pendingBindings.shaderParamsData.Resize(shaderParamsTypes.size() * sizeof(rhi::DeviceAddress), Byte(0u));

	for (SizeType i = 0; i < shaderParamsTypes.size(); ++i)
	{
		const lib::HashedString& paramType = shaderParamsTypes[i];

		const auto foundParamIt = std::find_if(m_boundShaderParams.cbegin(), m_boundShaderParams.cend(), [&paramType](const BoundShaderParam& param) { return param.type == paramType; });
		SPT_CHECK_MSG(foundParamIt != m_boundShaderParams.cend(), "Shader param of type {} not bound for pipeline {}!", paramType.GetData(), pipeline->GetRHI().GetName().GetData());

		if (foundParamIt != m_boundShaderParams.cend())
		{
			const SizeType paramOffset = i * sizeof(rhi::DeviceAddress);
			std::memcpy(pendingBindings.shaderParamsData.data() + paramOffset, &foundParamIt->address, sizeof(rhi::DeviceAddress));

			SPT_LOG_INFO(PipelinePendingState, "Marked shader param of type: {} with address: {} as dirty for pipeline: {}.", paramType.GetData(), foundParamIt->address, pipeline->GetRHI().GetName().GetData());
		}
	}

	return pendingBindings;
}

} // spt::rdr
