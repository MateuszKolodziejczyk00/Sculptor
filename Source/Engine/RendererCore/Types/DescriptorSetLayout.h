#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIDescriptorSetLayoutImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rdr
{

struct DescriptorBindingInfo
{
	rhi::EDescriptorType descriptorType = rhi::EDescriptorType::None;
	Uint32               arraySize      = 0u;
};


class RENDERER_CORE_API DescriptorSetLayout : public RendererResource<rhi::RHIDescriptorSetLayout>
{
protected:

	using ResourceType = RendererResource<rhi::RHIDescriptorSetLayout>;

public:

	explicit DescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def);

	DescriptorBindingInfo GetBindingInfo(Uint32 bindingIdx) const;

private:

	void InitializeBindingInfos(const rhi::DescriptorSetDefinition& def);

	lib::DynamicArray<DescriptorBindingInfo> m_bindingInfos;
};

} // spt::rdr
