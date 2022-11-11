#include "RGResources/RGNode.h"

namespace spt::rg
{

RGNode::RGNode()
{ }

void RGNode::Execute(const lib::SharedPtr<CommandRecorder>& recorder)
{
	OnExecute(recorder);
}

} // spt::rg
