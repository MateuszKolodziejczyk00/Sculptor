#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{

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
constexpr lib::String GetShaderTypeName<Int32>()
{
	return lib::String("int");
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
constexpr lib::String GetShaderTypeName<math::Matrix4f>()
{
	return lib::String("float4x4");
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

template<typename TStruct>
constexpr lib::String AddShaderStruct()
{
	return lib::String("[[shader_struct(") + TStruct::GetStructName() + ")]]";
}

} // shader_translator

} // spt::rdr
