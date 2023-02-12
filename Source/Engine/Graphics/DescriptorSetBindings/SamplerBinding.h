#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RHICore/RHISamplerTypes.h"


namespace spt::gfx
{

template<rhi::SamplerDefinition samplerDefinition>
class ImmutableSamplerBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit ImmutableSamplerBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		// Do nothing
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("SamplerState ") + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::ImmutableSampler;
	}

	static constexpr std::optional<rhi::SamplerDefinition> GetImmutableSamplerDef()
	{
		return std::make_optional(samplerDefinition);
	}
};

} // spt::gfx