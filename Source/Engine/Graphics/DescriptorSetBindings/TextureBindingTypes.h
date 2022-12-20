#pragma once

#include "SculptorCoreTypes.h"

namespace spt::gfx
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

} // priv

} // spt::gfx
