#pragma once

#include "UICoreMacros.h"
#include "SculptorCoreTypes.h"
#include "UITypes.h"


namespace spt::ui
{

class UI_CORE_API UIContextManager
{
public:

	static void			SetGlobalContext(UIContext context);

	static UIContext	GetGlobalContext();
};

}