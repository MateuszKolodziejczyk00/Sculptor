#pragma once

namespace spt::rhi
{

enum class ECompareOp
{
	None,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
	Equal,
	NotEqual,
	Always,
	Never
};

} // spt::rhi