#include "DescriptorSetStateTypes.h"
#include "ShaderMetaData.h"
#include "Types/Buffer.h"
#include "Types/DescriptorSetLayout.h"
#include "ResourcesManager.h"
#include "Types/Sampler.h"
#include "Renderer.h"
#include "Types/DescriptorHeap.h"
#include "RHIBridge/RHIImpl.h"
#include "RHIBridge/RHILimitsImpl.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetStateLayoutsRegistry =============================================================

DescriptorSetStateLayoutsRegistry& DescriptorSetStateLayoutsRegistry::Get()
{
	static DescriptorSetStateLayoutsRegistry instance;
	return instance;
}

void DescriptorSetStateLayoutsRegistry::CreateRegisteredLayouts()
{
	SPT_PROFILER_FUNCTION();

	for (const auto& [typeID, factoryMethod] : m_layoutFactoryMethods)
	{
		factoryMethod(*this);
	}
}

void DescriptorSetStateLayoutsRegistry::ReleaseRegisteredLayouts()
{
	m_layouts.clear();
}

void DescriptorSetStateLayoutsRegistry::RegisterFactoryMethod(DSStateTypeID dsTypeID, DescriptorSetStateLayoutFactoryMethod layoutFactoryMethod)
{
	m_layoutFactoryMethods.emplace(dsTypeID, std::move(layoutFactoryMethod));
}

void DescriptorSetStateLayoutsRegistry::RegisterLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef)
{
	if (ShouldCreateLayout(dsTypeID, name, layoutDef))
	{
		CreateLayout(dsTypeID, name, layoutDef);
	}
}

const lib::SharedPtr<DescriptorSetLayout>& DescriptorSetStateLayoutsRegistry::GetLayoutChecked(DSStateTypeID dsTypeID) const
{
	return m_layouts.at(dsTypeID);
}


DescriptorSetStateLayoutsRegistry::DescriptorSetStateLayoutsRegistry()
{
}

Bool DescriptorSetStateLayoutsRegistry::ShouldCreateLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef) const
{
	const Bool isRayTracingSettingAllowingCreation = rdr::Renderer::IsRayTracingEnabled() || !IsRayTracingLayout(dsTypeID, name, layoutDef);

	return isRayTracingSettingAllowingCreation;
}

void DescriptorSetStateLayoutsRegistry::CreateLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef)
{
	rhi::DescriptorSetDefinition descriptorSetLayoutDef;
	descriptorSetLayoutDef.bindings.reserve(layoutDef.bindings.size());

	std::transform(layoutDef.bindings.begin(), layoutDef.bindings.end(),
				   std::back_inserter(descriptorSetLayoutDef.bindings),
				   [bindingIdx = 0](const ShaderBindingMetaData& bindingMetaData) mutable
				   {
					   rhi::DescriptorSetBindingDefinition bindingDef;
					   bindingDef.bindingIdx = bindingIdx++;
					   bindingDef.descriptorType = bindingMetaData.type;
					   bindingDef.descriptorCount = bindingMetaData.arraySize;
					   bindingDef.shaderStages = rhi::EShaderStageFlags::All;

					   if (lib::HasAnyFlag(bindingMetaData.flags, smd::EBindingFlags::PartiallyBound))
					   {
						   lib::AddFlags(bindingDef.flags, rhi::EDescriptorSetBindingFlags::PartiallyBound);
					   }

					   if (bindingMetaData.immutableSamplerDef)
					   {
						   const lib::SharedRef<Sampler> sampler = ResourcesManager::CreateSampler(*bindingMetaData.immutableSamplerDef);
						   bindingDef.immutableSampler = sampler->GetRHI();
					   }

					   return bindingDef;
				   });

	m_layouts[dsTypeID] = ResourcesManager::CreateDescriptorSetLayout(name, descriptorSetLayoutDef);
}

Bool DescriptorSetStateLayoutsRegistry::IsRayTracingLayout(DSStateTypeID dsTypeID, const RendererResourceName& name, const DescriptorSetStateLayoutDefinition& layoutDef) const
{
	return std::any_of(layoutDef.bindings.begin(), layoutDef.bindings.end(),
					   [](const ShaderBindingMetaData& binding)
					   {
						   return binding.type == rhi::EDescriptorType::AccelerationStructure;
					   });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorsStackAllocator ====================================================================

DescriptorStackAllocator::DescriptorStackAllocator(Uint32 stackSize)
	: m_descriptorRange(Renderer::GetDescriptorHeap().GetRHI().AllocateRange(stackSize))
{
}

DescriptorStackAllocator::~DescriptorStackAllocator()
{
	Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry::CreateLambda(
		[ds = m_descriptorRange]
		{
			rdr::DescriptorHeap& descriptorHeap = Renderer::GetDescriptorHeap();
			descriptorHeap.GetRHI().DeallocateRange(ds);
		}));

	m_descriptorRange = rhi::RHIDescriptorRange{};
}

rhi::RHIDescriptorRange DescriptorStackAllocator::AllocateRange(Uint32 size)
{
	const Uint32 alignment = rhi::RHI::GetDescriptorProps().descriptorsAlignment;
	const Uint32 alignedSize = math::Utils::RoundUp(size, alignment);
	
	const Uint32 offset = m_currentOffset.fetch_add(alignedSize);
	SPT_CHECK(offset + alignedSize <= m_descriptorRange.data.size());

	return rhi::RHIDescriptorRange
	{
		.data             = lib::Span<Byte>{m_descriptorRange.data.data() + offset, alignedSize},
		.allocationHandle = {},
		.heapOffset       = m_descriptorRange.heapOffset + offset,
	};
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ConstantsAllocator ============================================================================

ConstantsAllocator::ConstantsAllocator(Uint32 stackSize)
{
	SPT_CHECK(stackSize > 0u);

	const rhi::BufferDefinition constantsBufferDef(stackSize, rhi::EBufferUsage::Uniform);
	const rhi::RHIAllocationInfo allocInfo{rhi::EMemoryUsage::CPUToGPU, rhi::EAllocationFlags::CreateMapped };
	m_buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Constants Memory Arena"), constantsBufferDef, allocInfo);

	m_buffersAlignment = static_cast<Uint32>(rhi::RHILimits::GetMinUniformBufferOffsetAlignment());
}

ConstantBufferAllocation ConstantsAllocator::Allocate(Uint32 size)
{
	const Uint32 alignedSize = (size + m_buffersAlignment - 1u) / m_buffersAlignment * m_buffersAlignment;
	const Uint32 allocationOffset = m_currentOffset.fetch_add(alignedSize);
	SPT_CHECK(allocationOffset + size < static_cast<Uint32>(m_buffer->GetRHI().GetSize()));

	return ConstantBufferAllocation
	{
		.buffer =  m_buffer,
		.offset = allocationOffset,
		.size   = size
	};
}

} // spt::rdr
