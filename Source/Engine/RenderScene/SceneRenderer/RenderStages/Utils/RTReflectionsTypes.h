#pragma once

#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct RTReflectionsViewData
{
	rg::RGTextureViewHandle reflectionsTextureHalfRes;
	rg::RGTextureViewHandle reflectionsTexture;

	rg::RGTextureViewHandle reflectionsInfluenceTexture;

	rg::RGTextureViewHandle temporalMomentsTexture;
	rg::RGTextureViewHandle spatialStdDevTexture;
	
	rg::RGTextureViewHandle geometryCoherenceTexture;
};

} // spt::rsc