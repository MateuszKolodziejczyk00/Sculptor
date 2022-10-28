#include "UILayers/UILayerTypes.h"

namespace spt::scui
{

UILayerID UILayerID::GenerateID()
{
	static SizeType id = 1;
	return UILayerID(id++);
}

} // spt::scui

