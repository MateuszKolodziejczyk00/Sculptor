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

template<typename TType>
constexpr Uint32 GetTypeAlignment()
{
	return alignof(TType);
}

template<>
constexpr Uint32 GetTypeAlignment<Bool>() // bool is aligned to 4 bytes in HLSL
{
	return 4;
}

} // shader_translator

} // spt::rdr
