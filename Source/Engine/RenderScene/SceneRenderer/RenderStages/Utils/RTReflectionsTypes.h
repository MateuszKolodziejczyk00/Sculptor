#pragma once

#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct RTReflectionsViewData
{
	rg::RGTextureViewHandle reflectionsTexture;

	rg::RGTextureViewHandle reflectionsInfluenceTexture;
	rg::RGTextureViewHandle reservoirsBrightnessTexture;
};

} // spt::rsc