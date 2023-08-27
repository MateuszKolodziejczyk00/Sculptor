#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "UILayers/UIViewTypes.h"


namespace spt::scui
{

struct ViewDefinition
{
	ViewDefinition()
		: minimumSize(0.0f, 0.0f)
	{ }

	explicit ViewDefinition(const lib::HashedString& inName)
		: name(inName)
		, minimumSize(0.0f, 0.0f)
	{ }

	lib::HashedString name;

	math::Vector2u minimumSize;
};


class SCUI_API UIView
{
public:

	explicit UIView(const ViewDefinition& definition);

	UIViewID GetID() const;

	EViewDrawResultActions Draw(const UIViewDrawParams& params);

	const lib::HashedString& GetName() const;

	const lib::HashedString& AddChild(lib::SharedRef<UIView> child);

	void RemoveChild(UIViewID viewID);
	void RemoveChild(const lib::SharedPtr<UIView>& view);

protected:

	virtual void BuildDefaultLayout(ImGuiID dockspaceID) const;
	virtual void DrawUI();

	lib::HashedString CreateUniqueName(const lib::HashedString& name);

private:
	
	lib::HashedString	m_name;
	UIViewID			m_id;

	math::Vector2f 		m_minimumSize;

	UIViewsContainer m_children;
};

} // spt::scui