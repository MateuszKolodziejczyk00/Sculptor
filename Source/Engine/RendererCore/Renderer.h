#pragma once

#include "RendererCoreMacros.h"


namespace spt::renderer
{

class RENDERER_CORE_API Renderer
{
public:

	static void			Initialize();
	static void			Shutdown();
};

}