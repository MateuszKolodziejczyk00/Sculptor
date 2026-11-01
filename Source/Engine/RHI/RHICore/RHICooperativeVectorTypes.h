#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class ECooperativeVectorType : Uint8
{
	Float32,
	Float16
};


enum class ECooperativeVectorLayout : Uint8
{
	RowMajor,
	ColumnMajor,
	InferencingOptimal,
	TrainingOptimal
};


struct ConvertCooperativeMatrixDefinition
{
	lib::Span<const Byte> srcData;
	lib::Span<Byte>       dstData;

	ECooperativeVectorType srcType = ECooperativeVectorType::Float32;
	ECooperativeVectorType dstType = ECooperativeVectorType::Float32;

	Uint32 numRows    = 0u;
	Uint32 numColumns = 0u;

	ECooperativeVectorLayout srcLayout = ECooperativeVectorLayout::RowMajor;
	ECooperativeVectorLayout dstLayout = ECooperativeVectorLayout::RowMajor;

	Uint32 srcStride = 0u;
	Uint32 dstStride = 0u;
};

} // spt::rhi
