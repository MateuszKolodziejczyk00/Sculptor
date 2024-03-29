#pragma once

#include "SculptorCoreTypes.h"

struct ImGuiContext;
using ImGuiID = unsigned int;


namespace spt::ui
{

class UIContext
{
public:

	UIContext()
		: m_context(nullptr)
	{ }

	UIContext(ImGuiContext* context)
		: m_context(context)
	{ }

	Bool IsValid() const
	{
		return m_context != nullptr;
	}

	ImGuiContext* GetHandle() const
	{
		return m_context;
	}

	void Reset()
	{
		m_context = nullptr;
	}

private:

	ImGuiContext* m_context;
};

} // spt::ui