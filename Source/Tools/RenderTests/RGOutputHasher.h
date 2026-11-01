#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"

#if RG_ENABLE_TEST_MODE

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rg::test_mode
{

namespace impl
{
class OutputsHasherDecorator;
} // impl


struct OutputHash
{
	lib::HashedString name;
	Uint64            hash = 0u;
};


struct PassOutputsHashes
{
	lib::HashedString     name;
	lib::Span<OutputHash> outputs;
};


class RENDER_GRAPH_API OutputsHasher
{
public:

	OutputsHasher();

	void Attach(RenderGraphBuilder& graphBuilder);

	lib::Span<PassOutputsHashes> ComputeHashes(lib::MemoryArena& outputScratch) const;

private:

	lib::SharedPtr<impl::OutputsHasherDecorator> m_decorator;
};

} // spt::rg::test_mode

#endif // RG_ENABLE_TEST_MODE
