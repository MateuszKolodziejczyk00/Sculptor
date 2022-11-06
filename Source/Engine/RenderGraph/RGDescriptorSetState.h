#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{

class RGDependenciesBuilder
{
public:

	RGDependenciesBuilder() = default;

};


template<typename TInstanceType>
class RGDescriptorSetState abstract : public rdr::DescriptorSetState
{
public:

	RGDescriptorSetState() = default;

	void BuildRGDependencies(RGDependenciesBuilder& builder)
	{
		TInstanceType& self = SelfAsInstance();
		const auto& bindingsBegin = self.GetBindingsBegin();
		rdr::bindings_refl::ForEachBinding(bindingsBegin, [&builder]<typename TBindingHandle>(const TBindingHandle& bindingHandle)
		{
			using BindingType = typename TBindingHandle::BindingType;

			constexpr Bool supportsRG = (requires(const BindingType& binding) { binding.BuildRGDependencies(std::declval<RGDependenciesBuilder>()); });

			SPT_STATIC_CHECK_MSG(supportsRG, "binding doesn't support Render Graph");
			if constexpr (supportsRG)
			{
				const BindingType& binding = bindingHandle.GetBinding();
				binding.BuildRGDependencies(builder);
			}
		});
	}

private:

	TInstanceType& SelfAsInstance()
	{
		return *static_cast<TInstanceType*>(this);
	}
};

} // spt::rg