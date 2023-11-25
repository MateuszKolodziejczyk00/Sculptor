#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class EBufferUsage : Flags32
{
	None							= 0,
	TransferSrc						= BIT(0),
	TransferDst						= BIT(1),
	UniformTexel					= BIT(2),
	StorageTexel					= BIT(3),
	Uniform							= BIT(4),
	Storage							= BIT(5),
	Index							= BIT(6),
	Vertex							= BIT(7),
	Indirect						= BIT(8),
	DeviceAddress					= BIT(9),
	AccelerationStructureStorage	= BIT(10),
	ASBuildInputReadOnly			= BIT(11),
	ShaderBindingTable				= BIT(12),

	LAST
};


enum EBufferFlags : Flags8
{
	None						= 0,
	WithVirtualSuballocations	= BIT(0)
};


struct BufferDefinition
{
	explicit BufferDefinition(Uint64 inSize = 0, EBufferUsage inUsage = EBufferUsage::None, EBufferFlags inFlags = EBufferFlags::None)
		: size(inSize)
		, usage(inUsage)
		, flags(inFlags)
	{ }

	Uint64			size;
	EBufferUsage	usage;
	EBufferFlags	flags;
};

} // spt::rhi
