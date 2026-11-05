#pragma once

#include "SculptorCore.h"
#include "Utility/String/String.h"
#include "Containers/StaticArray.h"
#include "Utility/Hash.h"

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

	static RuntimeTypeInfo CreateFromName(lib::StringView name)
	{
		return RuntimeTypeInfo{ name, FNV1a::Hash({ name.data(), name.size() }) };
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
		// MSVC signature: "const char *__cdecl spt::lib::TypeInfo<[struct |class |enum |union ]TypeName>::GetNameInternal(void)"
		lib::StringView name = GetNameInternal();
		name.remove_prefix(name.find('<') + 1u);

		for (const lib::StringView keyword : { lib::StringView("struct "), lib::StringView("class "), lib::StringView("enum "), lib::StringView("union ") })
		{
			if (name.starts_with(keyword))
			{
				name.remove_prefix(keyword.size());
				break;
			}
		}

		SizeType end   = 0u;
		Uint32   depth = 0u;

		for (; end < name.size(); ++end)
		{
			if (name[end] == '<')
			{
				depth++;
			}
			else if (name[end] == '>')
			{
				if (depth == 0u)
				{
					break;
				}

				depth--;
			}
		}

		return name.substr(0u, end);
	}

	static constexpr Uint32 GetTypeNameLength()
	{
		return static_cast<Uint32>(GetTypeName().size());
	}

	using TypeNameStorage = lib::StaticArray<char, GetTypeNameLength() + 1u>;

	static constexpr lib::StaticArray<char, GetTypeNameLength() + 1u> ConstructTypeNameStorage()
	{
		TypeNameStorage nameStorage{};
		const lib::StringView typeName = GetTypeName();
		for (SizeType i = 0u; i < typeName.size(); ++i)
		{
			nameStorage[i] = typeName[i];
		}
		nameStorage[typeName.size()] = '\0';
		return nameStorage;
	}

	static constexpr TypeID GetID()
	{
		const lib::StringView typeName = GetTypeName();
		return FNV1a::Hash({ typeName.data(), typeName.size() });
	}

	static constexpr TypeNameStorage typeNameStorage = ConstructTypeNameStorage();

public:

	TypeInfo() = default;

	static constexpr lib::StringView name = typeNameStorage.data();
	static constexpr TypeID          id   = GetID();

	constexpr operator RuntimeTypeInfo() const
	{
		return RuntimeTypeInfo{ name, id };
	}

	constexpr operator TypeID() const
	{
		return id;
	}
};


template<>
struct Hasher<RuntimeTypeInfo>
{
	SizeType operator()(const RuntimeTypeInfo& typeInfo) const
	{
		return typeInfo.id;
	}
};

} // spt::lib
