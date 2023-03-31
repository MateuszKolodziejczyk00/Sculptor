#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "UILayers/UIViewTypes.h"


namespace spt::scui
{

struct ViewDefinition
{
	ViewDefinition() = default;

	explicit ViewDefinition(const lib::HashedString& inName)
		: name(inName)
	{ }

	lib::HashedString name;
};


class SCUI_API UIView
{
public:

	explicit UIView(const ViewDefinition& definition);

	UIViewID GetID() const;

	EViewDrawResultActions Draw(const UIViewDrawParams& params);

	const lib::HashedString& GetName() const;

	void AddChild(lib::SharedRef<UIView> child);

	void RemoveChild(UIViewID viewID);
	void RemoveChild(const lib::SharedPtr<UIView>& view);

protected:

	virtual void DrawUI();

private:
	
	lib::HashedString	m_name;
	UIViewID			m_id;

	UIViewsContainer m_children;
};

} // spt::scui