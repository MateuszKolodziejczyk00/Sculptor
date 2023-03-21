#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class EQueryType
{
	Timestamp,
	Statistics
};


enum class EQueryStatisticsType
{
	None			= 0,
	IAVertices		= BIT(0),
	IAPrimitives	= BIT(1),
	VSInvocations	= BIT(2),
	FSInvocations	= BIT(3),
	CSInvocations	= BIT(4),

	All = IAVertices | IAPrimitives | VSInvocations | FSInvocations | CSInvocations
};;


struct QueryPoolDefinition
{
public:

	QueryPoolDefinition()
		: queryType(EQueryType::Timestamp)
		, queryCount(0)
		, statisticsType(EQueryStatisticsType::None)
	{ }
	
	EQueryType				queryType;
	Uint32					queryCount;
	EQueryStatisticsType	statisticsType;
};

} // spt::rhi