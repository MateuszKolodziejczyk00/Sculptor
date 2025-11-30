#include "Blackboard.h"
#include "BlackboardSerialization.h"


namespace spt::lib
{

void Blackboard::Serialize(srl::Serializer& serializer)
{
	BlackboardSerializer::SerializeBlackboard(serializer, *this);
}

} // spt::lib
