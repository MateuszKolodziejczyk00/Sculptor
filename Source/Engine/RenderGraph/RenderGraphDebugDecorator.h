#pragma once
#include "RGResources/RGResourceHandles.h"


namespace spt::rdr
{
class RenderContext;
class CommandRecorder;
} // spt::rdr


namespace spt::rg
{

class RenderGraphBuilder;
struct RGDependeciesContainer;
struct RGExecutionContext;


class RenderGraphDebugDecorator
{
public:

	virtual ~RenderGraphDebugDecorator() = default;

	virtual void PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies) {};
	virtual void PostSubpassAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies) {};
};

} // spt::rg