#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class alignas(2) Real16 
{
public:

	Real16() = default;

private:

	char data[2];
};
static_assert(sizeof(Real16) == 2);
static_assert(alignof(Real16) == 2);

using Real16_1 = Real16;
using Real16_2 = math::Vector2<Real16>;
using Real16_3 = math::Vector3<Real16>;
using Real16_4 = math::Vector4<Real16>;


namespace shader_translator
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Shader Type Names =============================================================================

template<typename TType>
constexpr lib::String GetShaderTypeName()
{
	SPT_CHECK_NO_ENTRY(); // type not supported
	return lib::String();
}

template<>
constexpr lib::String GetShaderTypeName<Bool>()
{
	return lib::String("bool");
}

template<>
constexpr lib::String GetShaderTypeName<Int16>()
{
	return lib::String("int16_t");
}

template<>
constexpr lib::String GetShaderTypeName<Int32>()
{
	return lib::String("int");
}

template<>
constexpr lib::String GetShaderTypeName<Uint16>()
{
	return lib::String("uint16_t");
}

template<>
constexpr lib::String GetShaderTypeName<Uint32>()
{
	return lib::String("uint");
}

template<>
constexpr lib::String GetShaderTypeName<Real32>()
{
	return lib::String("float");
}

template<>
constexpr lib::String GetShaderTypeName<Uint64>()
{
	return lib::String("uint64_t");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector2f>()
{
	return lib::String("float2");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector3f>()
{
	return lib::String("float3");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector4f>()
{
	return lib::String("float4");
}

template<>
constexpr lib::String GetShaderTypeName<Real16_1>()
{
	return lib::String("half");
}

template<>
constexpr lib::String GetShaderTypeName<Real16_2>()
{
	return lib::String("half2");
}

template<>
constexpr lib::String GetShaderTypeName<Real16_3>()
{
	return lib::String("half3");
}

template<>
constexpr lib::String GetShaderTypeName<Real16_4>()
{
	return lib::String("half4");
}

template<>
constexpr lib::String GetShaderTypeName<math::Matrix2f>()
{
	return lib::String("float2x2");
}

template<>
constexpr lib::String GetShaderTypeName<math::Matrix3f>()
{
	return lib::String("float3x3");
}

template<>
constexpr lib::String GetShaderTypeName<math::Matrix4f>()
{
	return lib::String("float4x4");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector2i>()
{
	return lib::String("int2");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector3i>()
{
	return lib::String("int3");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector4i>()
{
	return lib::String("int4");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector16_2u>()
{
	return lib::String("uint16_t2");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector16_3u>()
{
	return lib::String("uint16_t3");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector16_4u>()
{
	return lib::String("uint16_t4");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector2u>()
{
	return lib::String("uint2");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector3u>()
{
	return lib::String("uint3");
}

template<>
constexpr lib::String GetShaderTypeName<math::Vector4u>()
{
	return lib::String("uint4");
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

template<typename TType>
concept CShaderStruct = std::is_same_v<decltype(TType::GetStructName()), const char*>;


template<typename TType>
constexpr Bool IsShaderStruct()
{
	return CShaderStruct<TType>;
}

template<typename TType>
constexpr lib::String DefineType()
{
	lib::String code;
	// if TType is struct defined in C++, add it's definition to shader code
	if constexpr (CShaderStruct<TType>)
	{
		code += lib::String("[[shader_struct(") + TType::GetStructName() + ")]]";
	}
	return code;
}

template<typename TType>
constexpr lib::String GetTypeName()
{
	if constexpr (CShaderStruct<TType>)
	{
		return lib::String(TType::GetStructName());
	}
	else
	{
		return GetShaderTypeName<TType>();
	}
}


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
constexpr Uint32 GetMaxMemberAlignment()
{
	if constexpr (IsHeadMember<TShaderStructMemberMetaData>())
	{
		return GetMaxMemberAlignment<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>();
	}
	else if constexpr (IsTailMember<TShaderStructMemberMetaData>())
	{
		return 16u; // 16 is a min alignment for HLSL structures
	}
	else
	{

		return std::max(
			TShaderStructMemberMetaData::s_hlsl_memberAlignment,
			GetMaxMemberAlignment<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>());
	}
}

} // priv

template<typename TType>
constexpr Uint32 HLSLAlignOf()
{
	if constexpr (lib::CContainer<TType>)
	{
		using TArrayTraits = lib::StaticArrayTraits<TType>;
		using TElemType = typename TArrayTraits::Type;

		const Uint32 elementAlignment = HLSLAlignOf<TElemType>();
		return elementAlignment < 16u ? 16u : elementAlignment; // HLSL requires array elements to be aligned to 16 bytes
	}
	else if constexpr (CShaderStruct<TType>)
	{
		return priv::GetMaxMemberAlignment<typename TType::HeadMemberMetaData>();
	}
	else
	{
		return alignof(TType);
	}
}

template<>
constexpr Uint32 HLSLAlignOf<Bool>() // bool is aligned to 4 bytes in HLSL
{
	return 4;
}

template<typename TType>
constexpr Uint32 HLSLSizeOf()
{
	if constexpr (lib::CContainer<TType>)
	{
		using TArrayTraits = lib::StaticArrayTraits<TType>;
		using TElemType = typename TArrayTraits::Type;

		constexpr Uint32 typeSize = HLSLSizeOf<TElemType>();
		constexpr Uint32 elemSize = typeSize >= 16u ? typeSize : 16u; // HLSL requires array elements to be aligned to 16 bytes

		return elemSize * TArrayTraits::Size;
	}
	else if constexpr (CShaderStruct<TType>)
	{
		constexpr Uint32 alignment = HLSLAlignOf<TType>();
		using TLastMemberMetaData = typename TType::HeadMemberMetaData::PrevMemberMetaDataType;

		SPT_STATIC_CHECK_MSG(alignment != 0u, "Ivalid alignment");
		constexpr Uint32 missingAlignment = (alignment - (TLastMemberMetaData::s_hlsl_memberOffset + TLastMemberMetaData::s_hlsl_memberSize) % alignment) % alignment;

		return TLastMemberMetaData::s_hlsl_memberOffset + TLastMemberMetaData::s_hlsl_memberSize + missingAlignment;
	}
	else
	{
		return sizeof(TType);
	}
}

template<>
constexpr Uint32 HLSLSizeOf<Bool>()
{
	return 4;
}

template<typename TType>
void CopyCPPToHLSL(const TType& cppData, lib::Span<Byte> hlslData);

namespace priv
{

template<typename TShaderStructMemberMetaData>
void CopyCPPMembersToHLSL(lib::Span<const Byte> cppData, lib::Span<Byte> hlslData)
{
	if constexpr (!IsTailMember<TShaderStructMemberMetaData>())
	{
		CopyCPPMembersToHLSL<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(cppData, hlslData);
	}

	using TMemberType = typename TShaderStructMemberMetaData::UnderlyingType;

	if constexpr (!IsHeadMember<TShaderStructMemberMetaData>())
	{
		constexpr Uint32 hlslMemberOffset = TShaderStructMemberMetaData::s_hlsl_memberOffset;
		constexpr Uint32 hlslMemberSize = TShaderStructMemberMetaData::s_hlsl_memberSize;

		constexpr Uint32 cppMemberOffset = TShaderStructMemberMetaData::s_cpp_memberOffset;
		constexpr Uint32 cppMemberSize = TShaderStructMemberMetaData::s_cpp_memberSize;

		SPT_STATIC_CHECK(cppMemberSize == sizeof(TMemberType));

		CopyCPPToHLSL(*reinterpret_cast<const TMemberType*>(&cppData[cppMemberOffset]),
					  hlslData.subspan(hlslMemberOffset, hlslMemberSize));
	}
}

} // priv

template<typename TType>
void CopyCPPToHLSL(const TType& cppData, lib::Span<Byte> hlslData)
{
	if constexpr (lib::CContainer<TType>)
	{
		using TArrayTraits = lib::StaticArrayTraits<TType>;
		using TElemType = typename TArrayTraits::Type;

		constexpr Uint32 hlslTypeSize    = HLSLSizeOf<TElemType>();
		constexpr Uint32 hlslElementSize = hlslTypeSize >= 16u ? hlslTypeSize : 16u; // HLSL requires at least 16 bytes for each element of array

		for (SizeType idx = 0u; idx < TArrayTraits::Size; ++idx)
		{
			CopyCPPToHLSL(cppData[idx], hlslData.subspan(idx * hlslElementSize, hlslElementSize));
		}
	}
	else if constexpr (CShaderStruct<TType>)
	{
		priv::CopyCPPMembersToHLSL<typename TType::HeadMemberMetaData>(lib::Span<const Byte>(reinterpret_cast<const Byte*>(&cppData), sizeof(TType)),
																	   hlslData);
	}
	else
	{
		SPT_STATIC_CHECK_MSG(sizeof(TType) == HLSLSizeOf<TType>(), "Default implementation handles only types of the same size");
		SPT_CHECK(HLSLSizeOf<TType>() == hlslData.size());

		std::memcpy(hlslData.data(), &cppData, sizeof(TType));
	}
}

template<>
inline void CopyCPPToHLSL<Bool>(const Bool& cppData, lib::Span<Byte> hlslData)
{
	SPT_CHECK(hlslData.size() == sizeof(Uint32));
	*reinterpret_cast<Uint32*>(hlslData.data()) = (cppData ? 1u : 0u);
}

} // shader_translator

} // spt::rdr
