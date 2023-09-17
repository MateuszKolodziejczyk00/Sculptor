#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"


namespace spt::gfx
{

template<lib::Literal condition, typename TBindingType>
class ConditionalBinding : public TBindingType
{
protected:

	using Super = TBindingType;

public:

	explicit ConditionalBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		lib::String code = lib::String("#if ") + condition.Get() + "\n";
		code += Super::BuildBindingCode(name, bindingIdx);
		code += "\n#endif\n";

		return code;
	}
};

} // spt::gfx
