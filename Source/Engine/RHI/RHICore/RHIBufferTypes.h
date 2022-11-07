#pragma once


namespace spt::rhi
{

enum class EBufferUsage : Flags32
{
	None					= 0,
	TransferSrc				= BIT(0),
	TransferDst				= BIT(1),
	UniformTexel			= BIT(2),
	StorageTexel			= BIT(3),
	Uniform					= BIT(4),
	Storage					= BIT(5),
	Index					= BIT(6),
	Vertex					= BIT(7),
	Indirect				= BIT(8),
	DeviceAddress			= BIT(9),
	LAST
};


struct BufferDefinition
{
	explicit BufferDefinition(Uint64 inSize = 0, EBufferUsage inUsage = EBufferUsage::None)
		: size(inSize)
		, usage(inUsage)
	{ }

	Uint64			size;
	EBufferUsage	usage;
};

} // spt::rhi
