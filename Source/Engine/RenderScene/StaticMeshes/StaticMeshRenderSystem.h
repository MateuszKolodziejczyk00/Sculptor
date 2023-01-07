#pragma once

#include "RenderSystem.h"

namespace spt::rsc
{

class StaticMeshRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshRenderSystem();

};

} // spt::rsc
