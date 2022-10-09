#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{

namespace priv
{

template<typename TPrev, typename TType, lib::Literal name>
struct ShaderStructMemberMetaData
{
public:

	constexpr ShaderStructMemberMetaData() = default;

	using UnderlyingType			= TType;
	using PrevMemberMetaDataType	= TPrev;

	static constexpr Uint32 GetSize()
	{
		return sizeof(UnderlyingType);
	}

	static constexpr Uint32 GetAlignment()
	{
		if constexpr (std::is_same_v<TPrev, void>)
		{
			// 16 bytes alignment for first variable
			return 16;
		}
		else
		{
			constexpr Uint32 thisMemberSize = GetSize();

			if (thisMemberSize & (thisMemberSize - 1)) // is 2^n
			{
				// find and return next power of 2
				Uint32 alignment = thisMemberSize;
				alignment--;
				alignment |= alignment >> 1;
				alignment |= alignment >> 2;
				alignment |= alignment >> 4;
				alignment |= alignment >> 8;
				alignment |= alignment >> 16;
				alignment++;

				return alignment;
			}
			else
			{
				// if size is power of 2, just return it
				return thisMemberSize;
			}
		}
	}

	static constexpr const char* GetTypeName()
	{
		return shader_translator::GetShaderTypeName<UnderlyingType>();
	}

	static constexpr const char* GetVariableName()
	{
		return name.Get();
	}

	static constexpr lib::String GetVariableLineString()
	{
		return lib::String(GetTypeName()) + ' ' + GetVariableName() + ";\n";
	}
};


template<typename TPrev>
struct ShaderStructMemberMetaData<TPrev, void, "Head">
{
public:

	constexpr ShaderStructMemberMetaData() = default;

	using UnderlyingType			= void;
	using PrevMemberMetaDataType	= TPrev;
};

} // priv

#define BEGIN_SHADER_STRUCT(api, name) \
struct api name \
{ \
private: \
	using ThisClass = name; \
public: \
	static constexpr const char* GetStructName() \
	{ \
		return #name; \
	} \
	typedef rdr::priv::ShaderStructMemberMetaData<void, 


#define SHADER_STRUCT_FIELD(type, name) \
	type, #name> _MT_##name; \
	alignas(_MT_##name::GetAlignment()) type name = type{}; \
	typedef rdr::priv::ShaderStructMemberMetaData<_MT_##name,


#define END_SHADER_STRUCT() \
	void, "Head"> HeadMemberMetaData; \
};


namespace shader_translator
{

namespace priv
{

template<typename TShaderStructMemberMetaData>
consteval Bool IsHeadMember()
{
	using MemberType = typename TShaderStructMemberMetaData::UnderlyingType;
	return std::is_same_v<MemberType, void>;
}

template<typename TShaderStructMemberMetaData>
consteval Bool IsTailMember()
{
	using PrevMemberMetaDataType = typename TShaderStructMemberMetaData::PrevMemberMetaDataType;
	return std::is_same_v<PrevMemberMetaDataType, void>;
}

template<typename TShaderStructMemberMetaData>
constexpr void AppendMemberSourceCode(lib::String& inOutString)
{
	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		inOutString += TShaderStructMemberMetaData::GetVariableLineString();
	}

	if constexpr (IsTailMember<TShaderStructMemberMetaData>())
	{
		AppendMemberSourceCode<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>();
	}
}

template<typename TStruct>
constexpr void BuildShaderStructCodeImpl(lib::String& outStructCode)
{
	outStructCode += lib::String("struct") + TStruct::GetStructName() + "\n{\n";
	AppendMemberSourceCode<typename TStruct::HeadMemberMetaData>(outStructCode);
	outStructCode += "};\n";
}

template<typename TStruct>
consteval SizeType GetShaderStructCodeLength()
{
	lib::String structCode;
	priv::BuildShaderStructCodeImpl<TStruct>(structCode);
	return structCode.size();
}

} // priv

template<typename TStruct>
consteval auto BuildShaderStructCode()
{
	lib::String structCode;
	priv::BuildShaderStructCodeImpl<TStruct>(structCode);

	lib::StaticArray<char, priv::GetShaderStructCodeLength<TStruct>()> result;
	SPT_CHECK(structCode.size() == result.size());
	std::copy(std::cbegin(structCode), std::cend(structCode), std::begin(result));

	return result;
}

} // shader_translator

} // spt::rdr