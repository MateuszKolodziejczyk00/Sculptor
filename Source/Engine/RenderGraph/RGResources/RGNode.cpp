#include "RGResources/RGNode.h"

namespace spt::rg
{

RGNode::RGNode()
{ }

void RGNode::Execute(const lib::SharedPtr<CommandRecorder>& recorder)
{
	SPT_PROFILER_FUNCTION();

	executeFunction(recorder);
}

} // spt::rg
