#pragma once

#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

using PreRenderBasePassDelegate = lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/)>;
using RenderBasePassDelegate = lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/)>;


struct BasePassStageInput
{
	// GBuffer etc.
};


class BasePassRenderStage
{
public:

	BasePassRenderStage();

	void Render(rg::RenderGraphBuilder& graphBuilder, const BasePassStageInput&& input);
	
	PreRenderBasePassDelegate& GetPreRenderBasePassDelegate();
	RenderBasePassDelegate& GetRenderBasePassDelegate();

private:

	PreRenderBasePassDelegate m_preRenderBasePassDelegate;
	RenderBasePassDelegate m_renderBasePassDelegate;
};

} // spt::rsc