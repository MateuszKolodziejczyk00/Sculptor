#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderStructs/ShaderStructRegistration.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "MathUtils.h"

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
			// no alignment for first variable
			return 0;
		}
		else
		{
			constexpr Uint32 thisMemberSize = GetSize();
			return math::Utils::RoundUpToPowerOf2(thisMemberSize);
		}
	}

	static constexpr lib::String GetTypeName()
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
	static inline rdr::ShaderStructRegistration<ThisClass> _structRegistration; \
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
	if constexpr (!IsTailMember<TShaderStructMemberMetaData>())
	{
		AppendMemberSourceCode<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(inOutString);
	}
	
	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		inOutString += TShaderStructMemberMetaData::GetVariableLineString();
	}
}

template<typename TStruct>
constexpr void BuildShaderStructCodeImpl(lib::String& outStructCode)
{
	outStructCode += lib::String("struct ") + TStruct::GetStructName() + "\n{\n";
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

template<typename TStruct>
class ShaderStructRegistration : public sc::ShaderStructRegistration
{
public:

	explicit ShaderStructRegistration()
		: sc::ShaderStructRegistration(TStruct::GetStructName(), GetShaderSourceCode())
	{ }

private:

	static lib::String GetShaderSourceCode()
	{
		const auto sourceCodeArray = shader_translator::BuildShaderStructCode<TStruct>();
		return lib::String(std::cbegin(sourceCodeArray), std::cend(sourceCodeArray));
	}
};

} // spt::rdr