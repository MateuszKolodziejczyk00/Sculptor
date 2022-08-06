#pragma once

#include "SculptorCoreTypes.h"

namespace ImGui
{
struct ImGuiContext;
}


namespace spt::ui
{

class UIContext
{
public:

	UIContext()
		: m_context(nullptr)
	{ }

	UIContext(ImGui::ImGuiContext* context)
		: m_context(context)
	{ }

	Bool					IsValid() const
	{
		return m_context != nullptr;
	}

	ImGui::ImGuiContext*	GetHandle() const
	{
		return m_context;
	}


private:

	ImGui::ImGuiContext*	m_context;
};

}