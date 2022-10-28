#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::scui
{

struct SCUI_API UILayerID
{
public:

	static UILayerID GenerateID();

	UILayerID()
		: m_id(idxNone<SizeType>)
	{ }

	UILayerID(const UILayerID& rhs) = default;
	UILayerID& operator=(const UILayerID& rhs) = default;

	Bool operator==(const UILayerID& rhs) const
	{
		return m_id == rhs.m_id;
	}

	Bool IsValid() const
	{
		return m_id != idxNone<SizeType>;
	}

	void Reset()
	{
		m_id = idxNone<SizeType>;
	}

private:

	explicit UILayerID(SizeType id)
		: m_id(id)
	{ }

	SizeType m_id;
};

} // spt::scui