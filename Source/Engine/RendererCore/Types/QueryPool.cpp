#include "QueryPool.h"

namespace spt::rdr
{

QueryPool::QueryPool(const rhi::QueryPoolDefinition& definition)
{
	GetRHI().InitializeRHI(definition);
}

} // spt::rdr
