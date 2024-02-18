#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RGResources/RGTrackedObject.h"


namespace spt::rg
{

class RGDependenciesBuilder;


class RGDescriptorSetStateBase abstract : public rdr::DescriptorSetState
{
protected:

	using Super = rdr::DescriptorSetState;

public:

	RGDescriptorSetStateBase(const rdr::RendererResourceName& name, const rdr::DescriptorSetStateParams& params)
		: Super(name, params)
	{ }

	virtual void BuildRGDependencies(RGDependenciesBuilder& builder) const = 0;
};


template<typename TInstanceType>
class RGDescriptorSetState abstract : public RGDescriptorSetStateBase
{
protected:

	using Super = RGDescriptorSetStateBase;

public:

	RGDescriptorSetState(const rdr::RendererResourceName& name, const rdr::DescriptorSetStateParams& params)
		: Super(name, params)
	{ }

	virtual void BuildRGDependencies(RGDependenciesBuilder& builder) const override
	{
		const TInstanceType& self = SelfAsInstance();
		const auto& bindingsBegin = self.GetBindingsBegin();
		rdr::bindings_refl::ForEachBinding(bindingsBegin, [&builder]<typename TBindingType>(const TBindingType& binding)
		{
			constexpr Bool supportsRG = requires (const TBindingType& bindingInstance, RGDependenciesBuilder& builder) { bindingInstance.BuildRGDependencies(builder); };
			
			if constexpr (supportsRG)
			{
				binding.BuildRGDependencies(builder);
			}
		});
	}

private:

	const TInstanceType& SelfAsInstance() const
	{
		return *static_cast<const TInstanceType*>(this);
	}
};

} // spt::rg