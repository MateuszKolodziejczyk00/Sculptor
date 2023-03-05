#pragma once

#include "SculptorCoreTypes.h"
#include "RenderingDataRegistry.h"

namespace spt::rdr
{
class BottomLevelAS;
} // spt::rdr


namespace spt::rsc
{

struct BLASesProviderComponent
{
	// Entity that contains BLASesComponent
	RenderingDataEntityHandle entity;
};


struct BLASesComponent
{
	lib::DynamicArray<lib::SharedPtr<rdr::BottomLevelAS>> blases;
};

} // spt::rsc