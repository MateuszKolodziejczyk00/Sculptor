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
		return BuildBindingVariableCode(GetSamplerTypeName() + " " + name, bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		return { rdr::ShaderBindingMetaData(samplerDefinition) };
	}

private:

	static constexpr lib::String GetSamplerTypeName()
	{
		if constexpr (samplerDefinition.compareOp == rhi::ECompareOp::None)
		{
			return lib::String("SamplerState");
		}
		else
		{
			return lib::String("SamplerComparisonState");
		}
	}
};

} // spt::gfx