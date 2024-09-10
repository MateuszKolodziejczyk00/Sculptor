#pragma once

#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct RTReflectionsViewData
{
	rg::RGTextureViewHandle denoisedRefectionsTexture;
	rg::RGTextureViewHandle reflectionsTexture;

	rg::RGTextureViewHandle reflectionsInfluenceTexture;

	rg::RGTextureViewHandle temporalMomentsTexture;
	rg::RGTextureViewHandle spatialStdDevTexture;
	
	rg::RGTextureViewHandle geometryCoherenceTexture;

	Bool halfResReflections = false;
};

} // spt::rsc