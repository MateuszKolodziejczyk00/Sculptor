#pragma once

#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct RTReflectionsViewData
{
	rg::RGTextureViewHandle finalDiffuseGI;
	rg::RGTextureViewHandle finalSpecularGI;

	rg::RGTextureViewHandle reflectionsInfluenceTexture;
	rg::RGTextureViewHandle varianceEstimation;

	Bool halfResReflections = false;
};

} // spt::rsc