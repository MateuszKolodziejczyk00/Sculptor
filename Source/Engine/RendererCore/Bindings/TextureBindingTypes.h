#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{

namespace priv
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Dimensions ====================================================================================

enum class EBindingTextureDimensions
{
	Dim_1D,
	Dim_2D,
	Dim_3D
};

template<EBindingTextureDimensions dimensions>
constexpr lib::String GetTextureDimSuffix()
{
	switch (dimensions)
	{
	case priv::EBindingTextureDimensions::Dim_1D:	return lib::String("1D");
	case priv::EBindingTextureDimensions::Dim_2D:	return lib::String("2D");
	case priv::EBindingTextureDimensions::Dim_3D:	return lib::String("3D");

	default:
		
		SPT_CHECK_NO_ENTRY(); // dimension not supported
		return lib::String();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PixelFormatType ===============================================================================

template<typename TPixelFormatData>
constexpr lib::String GetPixelFormatShaderTypeName()
{
	SPT_CHECK_NO_ENTRY(); // type not supported
	return lib::String();
}

template<>
constexpr lib::String GetPixelFormatShaderTypeName<Real32>()
{
	return lib::String("float");
}

template<>
constexpr lib::String GetPixelFormatShaderTypeName<math::Vector2f>()
{
	return lib::String("float2");
}

template<>
constexpr lib::String GetPixelFormatShaderTypeName<math::Vector3f>()
{
	return lib::String("float3");
}

template<>
constexpr lib::String GetPixelFormatShaderTypeName<math::Vector4f>()
{
	return lib::String("float4");
}

} // priv

} // spt::rdr
