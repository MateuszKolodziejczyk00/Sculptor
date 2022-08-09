#include "UIContextManager.h"

namespace spt::ui
{

namespace priv
{

static UIContext	g_globalContext;

}

void UIContextManager::SetGlobalContext(UIContext context)
{
	priv::g_globalContext = context;
}

UIContext UIContextManager::GetGlobalContext()
{
	return priv::g_globalContext;
}

}
