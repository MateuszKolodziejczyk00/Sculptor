#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderStructs/ShaderStructRegistration.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "MathUtils.h"


namespace spt::rdr
{

namespace priv
{

template<typename TType>
struct ShaderStructMemberElementsNum
{
	static constexpr SizeType value = 1;
};


template<typename TType, SizeType elementsNum>
struct ShaderStructMemberElementsNum<lib::StaticArray<TType, elementsNum>>
{
	static constexpr SizeType value = elementsNum;
};


template<typename TType>
struct ShaderStructMemberElementType
{
	using Type = TType;
};


template<typename TType, SizeType elementsNum>
struct ShaderStructMemberElementType<lib::StaticArray<TType, elementsNum>>
{
	using Type = TType;
};


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

	static constexpr Uint32 GetElementsNum()
	{
		return ShaderStructMemberElementsNum<UnderlyingType>::value;
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
			if constexpr (GetElementsNum() == 1u)
			{
				constexpr Uint32 thisMemberSize = GetSize();
				constexpr Uint32 prevMemberEnd = TPrev::s_memberOffset + TPrev::s_memberSize;

				constexpr Uint32 alignofUnderlyingType = shader_translator::GetTypeAlignment<TType>();

				return ((prevMemberEnd % 16) + thisMemberSize) > 16 ? 16 : alignofUnderlyingType;
			}
			else
			{
				// handle HLSL arrays alignment
				// In HLSL each element of array is aligned to 16 bytes
				using ElementType = typename ShaderStructMemberElementType<UnderlyingType>::Type;
				static_assert(shader_translator::GetTypeAlignment<TType>() == 16u, "Invalid alignment of array element in C++");
				return 16u;
			}
		}
	}

	static constexpr Uint32 GetOffset()
	{
		if constexpr (std::is_same_v<TPrev, void>)
		{
			// 0 offset for first variable
			return 0;
		}
		else
		{
			constexpr Uint32 alignment = GetAlignment();
			constexpr Uint32 prevMemberOffset = TPrev::s_memberOffset;
			constexpr Uint32 prevMemberSize = TPrev::s_memberSize;

			constexpr Uint32 padding = (alignment - ((prevMemberOffset + prevMemberSize) % alignment)) % alignment;
			return prevMemberOffset + prevMemberSize + padding;
		}
	}

	static constexpr Uint32 s_memberSize      = GetSize();
	static constexpr Uint32 s_memberAlignment = GetAlignment();
	static constexpr Uint32 s_memberOffset    = GetOffset();

	static constexpr lib::String GetTypeName()
	{
		return shader_translator::GetTypeName<typename ShaderStructMemberElementType<UnderlyingType>::Type>();
	}

	static constexpr const char* GetVariableName()
	{
		return name.Get();
	}

	static constexpr lib::String GetVariableLineString()
	{
		constexpr SizeType elementsNum = ShaderStructMemberElementsNum<UnderlyingType>::value;

		lib::String line(GetTypeName() + ' ' + GetVariableName());
		if constexpr (elementsNum > 1)
		{
			line += '[';
			// quick way to support int to string constexpr conversion (as currently std::to_string is not constexpr and we don't need anything more)
			SPT_STATIC_CHECK(elementsNum <= 9);
			const char elementsNumChar = '0' + static_cast<char>(elementsNum);
			line += elementsNumChar;
			line += ']';
		}
		line += ";\n";

		return line;
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

#define BEGIN_ALIGNED_SHADER_STRUCT(alignment, name) \
struct name \
{ \
private: \
	using ThisClass = name; \
public: \
	static constexpr const char* GetStructName() \
	{ \
		return #name; \
	} \
	typedef rdr::priv::ShaderStructMemberMetaData<void, 


// Use 4 as default alignment (it can be still greater if any of members will have greater alignment)
#define BEGIN_SHADER_STRUCT(name) BEGIN_ALIGNED_SHADER_STRUCT(4, name)


#define SHADER_STRUCT_FIELD(type, name) \
	type, #name> _MT_##name; \
	alignas(_MT_##name::GetAlignment()) type name = type{}; \
	typedef rdr::priv::ShaderStructMemberMetaData<_MT_##name,


#define END_SHADER_STRUCT() \
	void, "Head"> HeadMemberMetaData; \
	private: \
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

template<typename TShaderStructMemberMetaData>
constexpr void AppendMemberInfo(lib::String& inOutString)
{
	if constexpr (!IsTailMember<TShaderStructMemberMetaData>())
	{
		AppendMemberInfo<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(inOutString);
	}
	
	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		inOutString += "[ ";
		inOutString += TShaderStructMemberMetaData::GetVariableName();
		inOutString += " - size: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_memberSize);
		inOutString += ", offset: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_memberOffset);
		inOutString += ", alignment: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_memberAlignment);
		inOutString += " ]";
	}
}

template<typename TStruct>
lib::String DumpStructInfo()
{
	lib::String dump;
	priv::AppendMemberInfo<typename TStruct::HeadMemberMetaData>(dump);
	return dump;
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