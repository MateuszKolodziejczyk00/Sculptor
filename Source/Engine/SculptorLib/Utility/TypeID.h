#pragma once

#include "SculptorCore.h"
#include "Utility/String/String.h"

namespace spt::lib
{

using TypeID = Uint64;


template<typename TType>
class TypeInfo
{
public:

	using Type   = TType;

private:

	static constexpr const char* GetNameInternal()
	{
#if _MSC_VER
		return __FUNCSIG__;
#endif // _MSC_VER
	};

	static constexpr lib::StringView GetTypeName()
	{
		const char* nameInternal = GetNameInternal();
		const char* start        = nameInternal;
		const char* end          = nameInternal;

		while (*end != '\0')
		{
			if (*end == ' ')
			{
				start = end + 1;
			}
			else if (*end == '>')
			{
				break;
			}

			++end;
		}

		return { start, static_cast<SizeType>(end - start) };
	}

	static constexpr TypeID GetID()
	{
		const lib::StringView typeName = GetTypeName();
		return FNV1a::Hash({ typeName.data(), typeName.size() });
	}

public:

	static constexpr lib::StringView name = GetTypeName();
	static constexpr TypeID          id   = GetID();
};

} // spt::lib