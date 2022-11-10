#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResources.h"


namespace spt::rg
{

class CommandRecorder;

using RGExecuteFunctionType = void(const lib::SharedPtr<CommandRecorder>& /*recorder*/);


class RENDER_GRAPH_API RGNode : public RGTrackedResource
{
public:

	RGNode();

	template<typename TCallable>
	void SetExecuteFunction(TCallable&& callable)
	{
		executeFunction = std::forward<TCallable>(callable);
	}

	void Execute(const lib::SharedPtr<CommandRecorder>& recorder);

private:

	std::function<RGExecuteFunctionType> executeFunction;

	struct TextureViewAccess
	{
		RGTextureView texture;
		ERGAccess prevAccess;
		ERGAccess access;
	};

	lib::DynamicArray<TextureViewAccess> m_textureViewAccesses;
};


using RGNodeHandle = RGResourceHandle<RGNode>;

} // spt::rg
