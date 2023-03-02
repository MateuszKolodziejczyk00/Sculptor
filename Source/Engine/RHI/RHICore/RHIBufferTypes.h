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


enum class EBufferSuballocationFlags : Flags8
{
	None = 0,
	PreferUpperAddress		= BIT(0),
	PreferMinMemory			= BIT(1),
	PreferFasterAllocation	= BIT(2),
	PreferMinOffset			= BIT(3)
};


struct SuballocationDefinition
{
	explicit SuballocationDefinition(Uint64 inSize = 0, Uint64 inAlignment = 1, EBufferSuballocationFlags inFlags = EBufferSuballocationFlags::None)
		: size(inSize)
		, alignment(inAlignment)
		, flags(inFlags)
	{ }

	Uint64						size;
	Uint64						alignment;
	EBufferSuballocationFlags	flags;
};


class RHISuballocation
{
public:

	RHISuballocation()
		: m_suballocationHandle(0)
		, m_offset(0)
	{ }
	
	RHISuballocation(Uint64 suballocationHandle, Uint64 offset)
		: m_suballocationHandle(suballocationHandle)
		, m_offset(offset)
	{ }
	
	Bool IsValid() const
	{
		return m_suballocationHandle != 0;
	}

	Uint64 GetOffset() const
	{
		return m_offset;
	}

	Uint64 GetSuballocationHandle() const
	{
		return m_suballocationHandle;
	}

	void Reset()
	{
		m_suballocationHandle = 0;
		m_offset = 0;
	}

private:

	Uint64 m_suballocationHandle;
	Uint64 m_offset;
};

} // spt::rhi
