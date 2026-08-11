#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

struct CloudscapeDefinition
{
	lib::SharedPtr<rdr::TextureView> weatherMap;
};

} // spt::rsc
