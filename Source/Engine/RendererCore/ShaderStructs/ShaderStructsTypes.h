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
struct StructTranslator
{
	static constexpr lib::String GetHLSLStructName()
	{
		SPT_CHECK_NO_ENTRY(); // type not supported
		return lib::String();
	}
};

template<>
struct StructTranslator<Bool>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("bool");
	}
};

template<>
struct StructTranslator<Int16>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("int16_t");
	}
};

template<>
struct StructTranslator<Int32>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("int");
	}
};

template<>
struct StructTranslator<Uint16>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint16_t");
	}
};

template<>
struct StructTranslator<Uint32>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint");
	}
};

template<>
struct StructTranslator<Real32>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float");
	}
};

template<>
struct StructTranslator<Uint64>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint64_t");
	}
};

template<>
struct StructTranslator<math::Vector2f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float2");
	}
};

template<>
struct StructTranslator<math::Vector3f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float3");
	}
};

template<>
struct StructTranslator<math::Vector4f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float4");
	}
};

template<>
struct StructTranslator<Real16_1>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("half");
	}
};

template<>
struct StructTranslator<Real16_2>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("half2");
	}
};

template<>
struct StructTranslator<Real16_3>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("half3");
	}
};

template<>
struct StructTranslator<Real16_4>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("half4");
	}
};

template<>
struct StructTranslator<math::Matrix2f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float2x2");
	}
};

template<>
struct StructTranslator<math::Matrix3f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float3x3");
	}
};

template<>
struct StructTranslator<math::Matrix4f>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("float4x4");
	}
};

template<>
struct StructTranslator<math::Vector2i>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("int2");
	}
};

template<>
struct StructTranslator<math::Vector3i>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("int3");
	}
};

template<>
struct StructTranslator<math::Vector4i>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("int4");
	}
};

template<>
struct StructTranslator<math::Vector16_2u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint16_t2");
	}
};

template<>
struct StructTranslator<math::Vector16_3u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint16_t3");
	}
};

template<>
struct StructTranslator<math::Vector16_4u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint16_t4");
	}
};

template<>
struct StructTranslator<math::Vector2u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint2");
	}
};

template<>
struct StructTranslator<math::Vector3u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint3");
	}
};

template<>
struct StructTranslator<math::Vector4u>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return lib::String("uint4");
	}
};

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
		return StructTranslator<TType>::GetHLSLStructName();
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
struct StructHLSLAlignmentEvaluator
{
	static constexpr Uint32 Alignment()
	{
		if constexpr (lib::CContainer<TType>)
		{
			using TArrayTraits = lib::StaticArrayTraits<TType>;
			using TElemType = typename TArrayTraits::Type;

			const Uint32 elementAlignment = StructHLSLAlignmentEvaluator<TElemType>::Alignment();
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
};


template<typename TType>
constexpr Uint32 HLSLAlignOf()
{
	return StructHLSLAlignmentEvaluator<TType>::Alignment();
}


template<typename TType>
struct StructHLSLSizeEvaluator
{
	static constexpr Uint32 Size()
	{
		if constexpr (lib::CContainer<TType>)
		{
			using TArrayTraits = lib::StaticArrayTraits<TType>;
			using TElemType = typename TArrayTraits::Type;

			constexpr Uint32 typeSize = StructHLSLSizeEvaluator<TElemType>::Size();
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
};

template<typename TType>
constexpr Uint32 HLSLSizeOf()
{
	return StructHLSLSizeEvaluator<TType>::Size();
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
struct StructCPPToHLSLTranslator
{
	static void Copy(const TType& cppData, lib::Span<Byte> hlslData)
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
};

template<typename TType>
void CopyCPPToHLSL(const TType& cppData, lib::Span<Byte> hlslData)
{
	StructCPPToHLSLTranslator<TType>::Copy(cppData, hlslData);
}

// Custom evaluators ===========================================================================

template<>
struct StructCPPToHLSLTranslator<Bool>
{
	static void Copy(const Bool& cppData, lib::Span<Byte> hlslData)
	{
		SPT_CHECK(hlslData.size() == sizeof(Uint32));
		*reinterpret_cast<Uint32*>(hlslData.data()) = (cppData ? 1u : 0u);
	}
};

template<>
struct StructHLSLSizeEvaluator<Bool>
{
	static constexpr Uint32 Size()
	{
		return 4; // bool is 4 bytes in HLSL
	}
};

template<>
struct StructHLSLAlignmentEvaluator<Bool>
{
	static constexpr Uint32 Alignment()
	{
		return 4; // bool is aligned to 4 bytes in HLSL
	}
};

} // shader_translator

} // spt::rdr
