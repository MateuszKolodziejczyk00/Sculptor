#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedResource.h"


namespace spt::rg
{

class CommandRecorder;

using RGExecuteFunctionType = void(const lib::SharedPtr<CommandRecorder>& /*recorder*/);


class RENDER_GRAPH_API RGNode : public RGTrackedResource
{
public:

	RGNode();

	void Execute(const lib::SharedPtr<CommandRecorder>& recorder);

protected:

	virtual void OnExecute(const lib::SharedPtr<CommandRecorder>& recorder) = 0;
};


template<typename TCallable>
class RGLambdaNode
{
public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedPtr<CommandRecorder>&>));

	explicit RGLambdaNode(TCallable callable)
		: m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedPtr<CommandRecorder>& recorder) override
	{
		m_callable(recorder);
	}

private:

	TCallable m_callable;
};

} // spt::rg
