#pragma once

#include "SculptorCoreTypes.h"


namespace spt::as
{

struct IESProfileDefinition
{
	Uint32 numLamps = 0;

	Real32 lumensPerLamp = 0.f;
	Real32 candelaMultiplier = 1.f;

	//Int32 photometricType = 0;
	//Int32 unitsType = 1;

	math::Vector3f sourceSize = {};

	Uint32 verticalAnglesNum   = 0u;
	Uint32 horizontalAnglesNum = 0u;

	lib::DynamicArray<Real32> verticalAngles;
	lib::DynamicArray<Real32> horizontalAngles;

	lib::DynamicArray<Real32> candela; // size = verticalAnglesNum * horizontalAnglesNum (horizontal-major)
};

} // spt::as
