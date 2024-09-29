#include "DescriptorSetStateTypes.h"
#include "ShaderMetaData.h"
#include "Types/Buffer.h"
#include "Types/DescriptorSetWriter.h"
#include "Types/DescriptorSetLayout.h"
#include "ResourcesManager.h"
#include "Types/Sampler.h"
#include "Renderer.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetUpdateContext ====================================================================

DescriptorSetUpdateContext::DescriptorSetUpdateContext(rhi::RHIDescriptorSet descriptorSet, DescriptorSetWriter& writer, const DescriptorSetLayout& dsLayout)
	: m_descriptorSet(descriptorSet)
	, m_writer(writer)
	, m_dsLayout(dsLayout)
	, m_baseBindingIdx(0)
{ }

void DescriptorSetUpdateContext::UpdateBuffer(Uint32 bindingIdx, const BufferView& buffer) const
{
	const Uint64 range = buffer.GetSize();
	SPT_CHECK(range > 0);

	m_writer.WriteBuffer(m_descriptorSet, CreateWriteDescriptorDefinition(bindingIdx), buffer, range);
}

void DescriptorSetUpdateContext::UpdateBuffer(Uint32 bindingIdx, const BufferView& buffer, const BufferView& countBuffer) const
{
	const Uint64 range = buffer.GetSize();
	SPT_CHECK(range > 0);

	m_writer.WriteBuffer(m_descriptorSet, CreateWriteDescriptorDefinition(bindingIdx), buffer, range, countBuffer);
}

void DescriptorSetUpdateContext::UpdateTexture(Uint32 bindingIdx, const lib::SharedRef<TextureView>& texture, Uint32 arrayIndex /*= 0*/) const
{
	m_writer.WriteTexture(m_descriptorSet, CreateWriteDescriptorDefinition(bindingIdx, arrayIndex), texture);
}

void DescriptorSetUpdateContext::UpdateAccelerationStructure(Uint32 bindingIdx, const lib::SharedRef<TopLevelAS>& tlas) const
{
	m_writer.WriteAccelerationStructure(m_descriptorSet, CreateWriteDescriptorDefinition(bindingIdx), tlas);
}

void DescriptorSetUpdateContext::AddBindingsOffset(Uint32 offset)
{
	m_baseBindingIdx += offset;
}

void DescriptorSetUpdateContext::RemoveBindingsOffset(Uint32 offset)
{
	SPT_CHECK(m_baseBindingIdx >= offset);
	m_baseBindingIdx -= offset;
}

rhi::WriteDescriptorDefinition DescriptorSetUpdateContext::CreateWriteDescriptorDefinition(Uint32 bindingIdx, Uint32 arrayIdx /*= 0*/) const
{
	const Uint32 finalBindingIdx = ComputeFinalBindingIdx(bindingIdx);

	const DescriptorBindingInfo bindingInfo = m_dsLayout.GetBindingInfo(finalBindingIdx);

	rhi::WriteDescriptorDefinition writeDefinition;
	writeDefinition.bindingIdx     = finalBindingIdx;
	writeDefinition.arrayElement   = arrayIdx;
	writeDefinition.descriptorType = bindingInfo.descriptorType;

	SPT_CHECK(arrayIdx < bindingInfo.arraySize);
	SPT_CHECK(writeDefinition.descriptorType != rhi::EDescriptorType::None);

	return writeDefinition;
}


Uint32 DescriptorSetUpdateContext::ComputeFinalBindingIdx(Uint32 bindingIdx) const
{
	return m_baseBindingIdx + bindingIdx;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetUpdateContext ====================================================================

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
						   lib::AddFlags(bindingDef.flags, rhi::EDescriptorSetBindingFlags::PartiallyBound, rhi::EDescriptorSetBindingFlags::UpdateAfterBind);
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

} // spt::rdr
