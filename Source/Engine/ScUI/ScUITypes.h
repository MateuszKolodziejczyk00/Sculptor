#pragma once

#include "SculptorCoreTypes.h"
#include "UIContext.h"
#include "RHI/RHICore/RHITextureTypes.h"


namespace spt::engn
{
class FrameContext;
} // spt::engn


namespace spt::scui
{

class Context
{
public:

	Context(engn::FrameContext& frame, ui::UIContext uiContext, rhi::EFragmentFormat backbufferFormat)
		: m_frame(frame)
		, m_uiContext(uiContext)
		, m_backBufferFormat(backbufferFormat)
	{}

	engn::FrameContext& GetCurrentFrame() const
	{
		return m_frame;
	}

	ui::UIContext GetUIContext() const
	{
		return m_uiContext;
	}

	rhi::EFragmentFormat GetBackBufferFormat() const
	{
		return m_backBufferFormat;
	}

private:

	ui::UIContext       m_uiContext;
	engn::FrameContext& m_frame;

	rhi::EFragmentFormat m_backBufferFormat;
};

} // spt::scui