#pragma once

#include "SculptorCoreTypes.h"
#include "RTTIMacros.h"


namespace spt::rtti
{

enum class RTTIPrimitiveType : Uint8
{
	Type,
	Enum,
};


struct RTTIStruct;
struct RTTIEnum;


struct RTTIPrimitive
{
	RTTIPrimitiveType    primitiveType = RTTIPrimitiveType::Type;
	lib::RuntimeTypeInfo type;
	Uint32               size = 0u;

	const RTTIStruct* AsStruct() const
	{
		return primitiveType == RTTIPrimitiveType::Type ? reinterpret_cast<const RTTIStruct*>(this) : nullptr;
	}

	const RTTIEnum* AsEnum() const
	{
		return primitiveType == RTTIPrimitiveType::Enum ? reinterpret_cast<const RTTIEnum*>(this) : nullptr;
	}
};


struct RTTIMemberMetaData
{
	const char* name  = nullptr;
};


struct RTTIMember
{
	lib::StringView      name;
	lib::RuntimeTypeInfo type;
	Uint32               offset = 0u;

	const void*          minValue = nullptr;
	const void*          maxValue = nullptr;

	lib::Span<const RTTIMemberMetaData> metaData;

	Bool HasMetaData(lib::StringView metaDataName) const
	{
		for (const RTTIMemberMetaData& elem : metaData)
		{
			if (metaDataName == elem.name)
			{
				return true;
			}
		}
		return false;
	}
};


struct RTTIStruct : public RTTIPrimitive
{
	RTTIStruct()
	{
		primitiveType = RTTIPrimitiveType::Type;
	}

	lib::Span<const RTTIMember> members;
};


struct RTTIEnumValue
{
	lib::StringView name;
	const void*     value = nullptr;
};


struct RTTIEnum : public RTTIPrimitive
{
	RTTIEnum()
	{
		primitiveType = RTTIPrimitiveType::Enum;
	}

	lib::Span<const RTTIEnumValue> values;
};


RTTI_API lib::MemoryArena& GetRTTIArena();

RTTI_API void RegisterType(const RTTIPrimitive& prim);

RTTI_API const RTTIPrimitive* GetType(lib::RuntimeTypeInfo type);


namespace builders
{

class RTTIMemberBuilder
{
public:

	explicit RTTIMemberBuilder(lib::StringView name, lib::RuntimeTypeInfo type, Uint32 offset)
		: m_member{ name, type, offset }
	{
	}

	template<typename TValueType>
	RTTIMemberBuilder& SetMinValue(const TValueType& value)
	{
		m_member.minValue = m_arena.Allocate<TValueType>(value);
		return *this;
	}

	template<typename TValueType>
	RTTIMemberBuilder& SetMaxValue(const TValueType& value)
	{
		m_member.maxValue = m_arena.Allocate<TValueType>(value);
		return *this;
	}

	template<typename... TMetaDatas>
	RTTIMemberBuilder& SetMetaData(const TMetaDatas&... metaDatas)
	{
		const RTTIMemberMetaData metaDatasArray[] = { metaDatas... };
		const lib::Span<const RTTIMemberMetaData> metaDatasSpan(metaDatasArray);
		const lib::Span<RTTIMemberMetaData> rttiMetaDatasData = m_arena.AllocateSpan<RTTIMemberMetaData>(metaDatasSpan.size());
		for (SizeType i = 0u; i < metaDatasSpan.size(); ++i)
		{
			rttiMetaDatasData[i] = metaDatasSpan[i];
		}
		m_member.metaData = rttiMetaDatasData;
		return *this;
	}

	RTTIMember Build() const
	{
		return m_member;
	}

private:

	lib::MemoryArena& m_arena = GetRTTIArena();
	RTTIMember        m_member;
};


class RTTIStructBuilder
{

public:

	explicit RTTIStructBuilder(lib::RuntimeTypeInfo type, Uint32 size)
	{
		m_struct = m_arena.Allocate<RTTIStruct>();
		m_struct->type = type;
		m_struct->size = size;
	}

	RTTIStructBuilder& SetMembers(lib::Span<const RTTIMember> members)
	{
		SPT_CHECK(m_struct->members.empty());
		lib::Span<RTTIMember> rttiMembersData = m_arena.AllocateSpan<RTTIMember>(members.size());
		for (SizeType i = 0u; i < members.size(); ++i)
		{
			rttiMembersData[i] = members[i];
		}
		m_struct->members = rttiMembersData;
		return *this;
	}

	const void Build() const
	{
		RegisterType(*m_struct);
	}

private:

	lib::MemoryArena& m_arena = GetRTTIArena();
	RTTIStruct*       m_struct;
};


class RTTIEnumValueBuilder
{
public:

	explicit RTTIEnumValueBuilder(lib::StringView name)
		: m_enumValue{ name }
	{
	}

	template<typename TValueType>
	RTTIEnumValueBuilder& SetValue(const TValueType& value)
	{
		m_enumValue.value = m_arena.Allocate<TValueType>(value);
		return *this;
	}

	const RTTIEnumValue Build() const
	{
		return m_enumValue;
	}

private:

	lib::MemoryArena& m_arena = GetRTTIArena();
	RTTIEnumValue     m_enumValue;
};


class RTTIEnumBuilder
{
public:

	explicit RTTIEnumBuilder(lib::RuntimeTypeInfo type, Uint32 size)
	{
		m_enum = m_arena.Allocate<RTTIEnum>();
		m_enum->type = type;
		m_enum->size = size;
	}

	RTTIEnumBuilder& SetValues(lib::Span<const RTTIEnumValue> values)
	{
		SPT_CHECK(m_enum->values.empty());
		lib::Span<RTTIEnumValue> rttiValuesData = m_arena.AllocateSpan<RTTIEnumValue>(values.size());
		for (SizeType i = 0u; i < values.size(); ++i)
		{
			rttiValuesData[i] = values[i];
		}
		m_enum->values = rttiValuesData;
		return *this;
	}

	const void Build() const
	{
		RegisterType(*m_enum);
	}

private:

	lib::MemoryArena& m_arena = GetRTTIArena();
	RTTIEnum*         m_enum;
};


} // builders


template<typename TType>
concept CRTTIStruct = std::is_class_v<TType> && requires(const TType* ptr) { RegisterRTTIType(ptr); };


template<typename TType>
struct RTTITypeRegistration
{
	explicit RTTITypeRegistration()
	{
		const TType* ptr = nullptr;
		RegisterRTTIType(ptr);
	}
};

} // spt::rtti


#define SPT_RTTI_TYPE(type) \
	inline const spt::rtti::RTTITypeRegistration<type> SPT_IDENTIFIER_NAME(TypeRegistration);
