#pragma once


namespace spt::renderer
{

class CurrentFrameContext
{
public:

	template<typename TReleaseFunctor>
	static void			SubmitDeferredRelease(TReleaseFunctor func)
	{
		// TODO
	}
};

}