#include "DescriptorSetLayout.h"


namespace spt::rdr
{

DescriptorSetLayout::DescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(def);
	GetRHI().SetName(name.Get());

	InitializeBindingInfos(def);
}

DescriptorBindingInfo DescriptorSetLayout::GetBindingInfo(Uint32 bindingIdx) const
{
	return bindingIdx < m_bindingInfos.size() ? m_bindingInfos[bindingIdx] : DescriptorBindingInfo{};
}

void DescriptorSetLayout::InitializeBindingInfos(const rhi::DescriptorSetDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	m_bindingInfos.reserve(def.bindings.size());

	const auto generateBindingInfo = [](const rhi::DescriptorSetBindingDefinition& binding) -> DescriptorBindingInfo
	{
		DescriptorBindingInfo info;
		info.descriptorType = binding.descriptorType;
		info.arraySize      = binding.descriptorCount;
		return info;
	};

	std::transform(def.bindings.begin(), def.bindings.end(), std::back_inserter(m_bindingInfos), generateBindingInfo);
}

} // spt::rdr
