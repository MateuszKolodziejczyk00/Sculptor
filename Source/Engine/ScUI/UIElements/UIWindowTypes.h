#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"


using ImGuiID = unsigned int;


namespace spt::scui
{

class SCUI_API CurrentWindowBuildingContext
{
public:

	static void		SetCurrentWindowDockspaceID(ImGuiID inDockspaceID);
	static ImGuiID	GetCurrentWindowDockspaceID();

private:

	CurrentWindowBuildingContext() = delete;

	static ImGuiID s_currentWindowDockspaceID;
};


class WindowBuildingScope
{
public:

	explicit WindowBuildingScope(lib::HashedString windowName)
		: m_prevDockspaceID(CurrentWindowBuildingContext::GetCurrentWindowDockspaceID())
	{
		CurrentWindowBuildingContext::SetCurrentWindowDockspaceID(static_cast<ImGuiID>(windowName.GetKey()));
	}

	~WindowBuildingScope()
	{
		CurrentWindowBuildingContext::SetCurrentWindowDockspaceID(m_prevDockspaceID);
	}

private:

	ImGuiID m_prevDockspaceID;
};

} // spt::scui
