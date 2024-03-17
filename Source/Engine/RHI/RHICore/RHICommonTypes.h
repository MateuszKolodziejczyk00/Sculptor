#pragma once

namespace spt::rhi
{

enum class ECompareOp : Uint8
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