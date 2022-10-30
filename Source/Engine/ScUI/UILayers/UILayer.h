#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "UILayers/UILayerTypes.h"


namespace spt::scui
{

struct LayerDefinition
{
	explicit LayerDefinition(const lib::HashedString& inName)
		: name(inName)
	{ }

	lib::HashedString name;
};


class SCUI_API UILayer
{
public:

	explicit UILayer(const LayerDefinition& definition);

	void SetID(UILayerID id);
	UILayerID GetID() const;

	void Draw();

	const lib::HashedString& GetName() const;

protected:

	virtual void DrawUI();

private:

	lib::HashedString	m_name;
	UILayerID			m_id;
};

} // spt::scui