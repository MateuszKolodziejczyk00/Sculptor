#pragma once

#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct RTReflectionsViewData
{
	rg::RGTextureViewHandle denoisedRefectionsTexture;
	rg::RGTextureViewHandle reflectionsTexture;
	rg::RGTextureViewHandle reflectionsInfluenceTexture;
	rg::RGTextureViewHandle spatioTemporalVariance;

	Bool halfResReflections = false;
};

} // spt::rsc