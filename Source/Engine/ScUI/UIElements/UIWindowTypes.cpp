#include "UIElements/UIWindowTypes.h"

namespace spt::scui
{

ImGuiID CurrentWindowBuildingContext::s_currentWindowDockspaceID = 0;

void CurrentWindowBuildingContext::SetCurrentWindowDockspaceID(ImGuiID inDockspaceID)
{
	s_currentWindowDockspaceID = inDockspaceID;
}

ImGuiID CurrentWindowBuildingContext::GetCurrentWindowDockspaceID()
{
	return s_currentWindowDockspaceID;
}

} // spt::scui
