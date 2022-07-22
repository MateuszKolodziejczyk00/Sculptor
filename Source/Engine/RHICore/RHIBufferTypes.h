#pragma once


namespace spt::rhicore
{

namespace EBufferUsage
{

enum Flags : Flags32
{
	TransferSrc				= BIT(0),
	TransferDst				= BIT(1),
	UniformTexel			= BIT(2),
	StorageTexel			= BIT(3),
	Uniform					= BIT(4),
	Storage					= BIT(5),
	Index					= BIT(6),
	Vertex					= BIT(7),
	Indirect				= BIT(8),
	DeviceAddress			= BIT(9)
};

}

enum class EBufferMemoryUsage
{
	GPUOnly,
	CPUOnly,
	CPUToGPU,
	GPUToCpu,
	CPUCopy
};

}
