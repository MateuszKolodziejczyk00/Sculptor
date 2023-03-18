#include "QueryPool.h"

namespace spt::rdr
{

QueryPool::QueryPool(const rhi::QueryPoolDefinition& definition)
{
	GetRHI().InitializeRHI(definition);
}

void QueryPool::Reset()
{
	GetRHI().Reset(0, GetRHI().GetQueryCount());
}

} // spt::rdr
