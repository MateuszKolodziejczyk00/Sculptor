#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class EQueryType
{
	Timestamp
};


struct QueryPoolDefinition
{
public:

	QueryPoolDefinition()
		: queryType(EQueryType::Timestamp)
		, queryCount(0)
	{ }
	
	EQueryType		queryType;
	Uint32			queryCount;
};

} // spt::rhi