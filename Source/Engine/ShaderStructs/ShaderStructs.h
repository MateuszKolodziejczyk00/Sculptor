#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "MathUtils.h"
#include "ShaderStructsRegistry.h"
#include "Utility/Templates/TypeStorage.h"


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

	using ElementType            = typename ShaderStructMemberElementType<TType>::Type;
	using UnderlyingType         = TType;
	using PrevMemberMetaDataType = TPrev;

	static constexpr Uint32 GetCPPSize()
	{
		return sizeof(UnderlyingType);
	}

	static constexpr Uint32 GetHLSLSize()
	{
		return shader_translator::HLSLSizeOf<UnderlyingType>();
	}

	static constexpr Uint32 GetElementsNum()
	{
		return ShaderStructMemberElementsNum<UnderlyingType>::value;
	}

	static constexpr Uint32 GetHLSLAlignment()
	{
		if constexpr (std::is_same_v<TPrev, void>)
		{
			return shader_translator::HLSLAlignOf<TType>();
		}
		else
		{
			if constexpr (GetElementsNum() == 1u)
			{
				return shader_translator::HLSLAlignOf<UnderlyingType>();
			}
			else
			{
				return shader_translator::HLSLAlignOf<ElementType>();
			}
		}
	}

	static constexpr Uint32 GetCPPAlignment()
	{
		return alignof(UnderlyingType);
	}

	static constexpr Uint32 GetHLSLOffset()
	{
		if constexpr (std::is_same_v<TPrev, void>)
		{
			// 0 offset for first variable
			return 0;
		}
		else
		{
			constexpr Uint32 alignment = GetHLSLAlignment();
			constexpr Uint32 prevMemberOffset = TPrev::s_hlsl_memberOffset;
			constexpr Uint32 prevMemberSize = TPrev::s_hlsl_memberSize;

			constexpr Uint32 padding = (alignment - ((prevMemberOffset + prevMemberSize) % alignment)) % alignment;
			return prevMemberOffset + prevMemberSize + padding;
		}
	}

	static constexpr Uint32 GetCPPOffset()
	{
		if constexpr (std::is_same_v<TPrev, void>)
		{
			// 0 offset for first variable
			return 0;
		}
		else
		{
			constexpr Uint32 alignment = GetCPPAlignment();
			constexpr Uint32 prevMemberOffset = TPrev::s_cpp_memberOffset;
			constexpr Uint32 prevMemberSize = TPrev::s_cpp_memberSize;
			constexpr Uint32 padding = (alignment - ((prevMemberOffset + prevMemberSize) % alignment)) % alignment;
			return prevMemberOffset + prevMemberSize + padding;
		}
	}

	static constexpr Uint32 s_hlsl_memberSize      = GetHLSLSize();
	static constexpr Uint32 s_hlsl_memberAlignment = GetHLSLAlignment();
	static constexpr Uint32 s_hlsl_memberOffset    = GetHLSLOffset();

	static constexpr Uint32 s_cpp_memberSize      = GetCPPSize();
	static constexpr Uint32 s_cpp_memberAlignment = GetCPPAlignment();
	static constexpr Uint32 s_cpp_memberOffset    = GetCPPOffset();

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
			SPT_STATIC_CHECK(elementsNum <= 99);
			const char elementsNumChar0 = elementsNum >= 10 ? '0' + static_cast<char>(elementsNum / 10) : ' ';
			const char elementsNumChar1 = '0' + static_cast<char>(elementsNum % 10);
			line += elementsNumChar0;
			line += elementsNumChar1;
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


#define BEGIN_SHADER_STRUCT(name) \
struct name \
{ \
private: \
	using ThisClass = name; \
public: \
	static constexpr const char* GetStructName() \
	{ \
		return #name; \
	} \
	Bool operator==(const name& other) const = default; \
	Bool operator!=(const name& other) const = default; \
	typedef rdr::priv::ShaderStructMemberMetaData<void, 


#define SHADER_STRUCT_FIELD(type, name) \
	type, #name> _MT_##name; \
	type name = type{}; \
	typedef rdr::priv::ShaderStructMemberMetaData<_MT_##name,


#define END_SHADER_STRUCT() \
	void, "Head"> HeadMemberMetaData; \
	private: \
	static inline rdr::ShaderStructRegistration<ThisClass> _structRegistration; \
};


namespace shader_translator
{

template<typename TType>
struct ShaderStructReferencer
{
	static constexpr void CollectReferencedStructs(lib::DynamicArray<lib::String>& references)
	{
		if constexpr (IsShaderStruct<TType>())
		{
			references.emplace_back(GetTypeName<TType>());
		}
	}
};


namespace priv
{

template<typename TShaderStructMemberMetaData>
constexpr void CollectReferencedStructs(lib::DynamicArray<lib::String>& references)
{
	if constexpr (!IsTailMember<TShaderStructMemberMetaData>())
	{
		CollectReferencedStructs<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(references);
	}
	
	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		using MemberElementType = typename TShaderStructMemberMetaData::ElementType;
		ShaderStructReferencer<MemberElementType>::CollectReferencedStructs(references);
	}
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
	lib::DynamicArray<lib::String> references;
	CollectReferencedStructs<typename TStruct::HeadMemberMetaData>(INOUT references);
	for (const lib::String& structName : references)
	{
		outStructCode += "[[shader_struct(";
		outStructCode += structName;
		outStructCode += ")]]";
	}

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
		inOutString += TShaderStructMemberMetaData::GetVariableName();
		inOutString += " - [ ";
		inOutString += "size: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_cpp_memberSize);
		inOutString += ", offset: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_cpp_memberOffset);
		inOutString += ", alignment: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_cpp_memberAlignment);
		inOutString += " ]";
		inOutString += "->";
		inOutString += "[ ";
		inOutString += "size: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_hlsl_memberSize);
		inOutString += ", offset: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_hlsl_memberOffset);
		inOutString += ", alignment: ";
		inOutString += std::to_string(TShaderStructMemberMetaData::s_hlsl_memberAlignment);
		inOutString += " ]\n";
	}
}

template<typename TStruct>
lib::String DumpStructInfo()
{
	lib::String dump;
	priv::AppendMemberInfo<typename TStruct::HeadMemberMetaData>(dump);
	return dump;
}

#if SHADER_STRUCTS_RTTI
template<typename TShaderStructMemberMetaData>
void AppendMemberRTTI(ShaderStructRTTI& rtti)
{
	if constexpr (!IsTailMember<TShaderStructMemberMetaData>())
	{
		AppendMemberRTTI<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(rtti);
	}
	
	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		const Uint32 structSize = shader_translator::HLSLSizeOf<typename TShaderStructMemberMetaData::ElementType>();

		ShaderStructMemberRTTI memberRTTI;
		memberRTTI.memberName     = TShaderStructMemberMetaData::GetVariableName();
		memberRTTI.memberTypeName = TShaderStructMemberMetaData::GetTypeName();
		memberRTTI.offset         = TShaderStructMemberMetaData::s_hlsl_memberOffset;
		memberRTTI.elementsNum    = TShaderStructMemberMetaData::GetElementsNum();
		memberRTTI.stride         = structSize;

		rtti.members.emplace_back(std::move(memberRTTI));
	}
}
template<typename TStruct>
ShaderStructRTTI BuildRTTI()
{
	ShaderStructRTTI rtti;
	rtti.structName = TStruct::GetStructName();
	rtti.size       = shader_translator::HLSLSizeOf<TStruct>();

	AppendMemberRTTI<typename TStruct::HeadMemberMetaData>(rtti);

	return rtti;
}
#endif // SHADER_STRUCTS_RTTI

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
class ShaderStructRegistration
{
public:

	explicit ShaderStructRegistration()
	{

		ShaderStructMetaData structMetaData(GetShaderSourceCode());

#if SHADER_STRUCTS_RTTI
		structMetaData.SetRTTI(shader_translator::priv::BuildRTTI<TStruct>());
#endif // SHADER_STRUCTS_RTTI

		ShaderStructsRegistry::RegisterStructMetaData(TStruct::GetStructName(), std::move(structMetaData));
	}

private:

	static lib::String GetShaderSourceCode()
	{
		const auto sourceCodeArray = shader_translator::BuildShaderStructCode<TStruct>();
		return lib::String(std::cbegin(sourceCodeArray), std::cend(sourceCodeArray));
	}
};


template<typename TStruct>
class HLSLStorage : private lib::GenericStorage<shader_translator::HLSLAlignOf<TStruct>(), shader_translator::HLSLSizeOf<TStruct>()>
{
protected:

	using Super = lib::GenericStorage<shader_translator::HLSLAlignOf<TStruct>(), shader_translator::HLSLSizeOf<TStruct>()>;

public:

	using Struct = TStruct;
	constexpr static Uint32 s_size = shader_translator::HLSLSizeOf<TStruct>();
	constexpr static Uint32 s_alignment = shader_translator::HLSLAlignOf<TStruct>();

	HLSLStorage() = default;

	HLSLStorage(const TStruct& value)
	{
		Set(value);
	}

	HLSLStorage& operator=(const TStruct& value)
	{
		Set(value);
		return *this;
	}

	void Set(const TStruct& value)
	{
		shader_translator::CopyCPPToHLSL(value, lib::Span<Byte>(Super::GetAddress(), s_size));
	}

	lib::Span<const Byte> GetHLSLDataSpan() const
	{
		return lib::Span<const Byte>(Super::GetAddress(), s_size);
	}
};


template<typename TShaderStructMemberMetaData>
constexpr SizeType ComputeStructHashImpl(lib::Span<const Byte> domainData)
{
	if constexpr (rdr::shader_translator::priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		return ComputeStructHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData);
	}
	else
	{
		using MemberType = typename TShaderStructMemberMetaData::UnderlyingType;

		static_assert(lib::isHashable<MemberType> || shader_translator::CShaderStruct<MemberType>);

		const MemberType& memberValue = *reinterpret_cast<const MemberType*>(domainData.data() + TShaderStructMemberMetaData::GetCPPOffset());

		if constexpr (rdr::shader_translator::priv::IsTailMember<TShaderStructMemberMetaData>())
		{
			if constexpr (shader_translator::CShaderStruct<MemberType>)
			{
				return ComputeStructHashImpl<typename MemberType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&memberValue), sizeof(MemberType)));
			}
			else
			{
				return lib::GetHash(memberValue);
			}
		}
		else
		{
			if constexpr (shader_translator::CShaderStruct<MemberType>)
			{
				return lib::HashCombine(ComputeStructHashImpl<typename MemberType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&memberValue), sizeof(MemberType))),
										ComputeStructHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData));
			}
			else
			{
				return lib::HashCombine(memberValue, ComputeStructHashImpl<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(domainData));
			}
		}
	}
}


template<typename TStruct>
SizeType ComputeStructHash(const TStruct& permutation)
{
	const lib::Span<const Byte> domainData(reinterpret_cast<const Byte*>(&permutation), sizeof(TStruct));
	return ComputeStructHashImpl<typename TStruct::HeadMemberMetaData>(domainData);
}


template<typename TStruct>
struct ShaderStructHasher
{
	SizeType operator()(const TStruct& structData) const
	{
		return ComputeStructHash(structData);
	}
};

} // spt::rdr
