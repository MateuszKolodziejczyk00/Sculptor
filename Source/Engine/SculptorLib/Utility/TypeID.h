#pragma once

#include "SculptorCore.h"
#include "Utility/String/String.h"

namespace spt::lib
{

using TypeID = Uint64;


struct RuntimeTypeInfo
{
	RuntimeTypeInfo() = default;

	RuntimeTypeInfo(lib::StringView name, TypeID id)
		: name(name)
		, id(id)
	{
	}

	Bool IsValid() const
	{
		return id != 0u;
	}

	Bool operator==(const RuntimeTypeInfo& other) const
	{
		return id == other.id;
	}

	Bool operator!=(const RuntimeTypeInfo& other) const
	{
		return !(*this == other);
	}

	lib::StringView name;
	TypeID          id   = 0u;
};


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

	constexpr operator RuntimeTypeInfo() const
	{
		return RuntimeTypeInfo{ name, id };
	}

public:

	static constexpr lib::StringView name = GetTypeName();
	static constexpr TypeID          id   = GetID();
};

} // spt::lib