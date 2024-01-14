#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::gfx::dbg
{

namespace EArgumentTypeCode
{
enum Type : Uint32
	{
		Invalid = 0,

		Float  = 1,
		Float2 = 2,
		Float3 = 3,
		Float4 = 4,
		
		Uint  = 5,
		Uint2 = 6,
		Uint3 = 7,
		Uint4 = 8,
		
		Int  = 9,
		Int2 = 10,
		Int3 = 11,
		Int4 = 12
	};
}

struct InvalidArgument {};
using DebugArgument = std::variant<
	InvalidArgument,

	Real32,
	math::Vector2f,
	math::Vector3f,
	math::Vector4f,

	Uint32,
	math::Vector2u,
	math::Vector3u,
	math::Vector4u,

	Int32,
	math::Vector2i,
	math::Vector3i,
	math::Vector4i
>;


class ShaderDebugCommandsStream
{
public:

	ShaderDebugCommandsStream(const lib::SharedRef<rdr::Buffer>& commands, const lib::SharedRef<rdr::Buffer>& size)
		: m_buffer(commands->GetRHI())
		, m_currentOffset(0)
		, m_bufferSize(rhi::RHIMappedBuffer<Uint32>(size->GetRHI())[0])
	{
	}

	template<typename TType>
	TType Read()
	{
		constexpr size_t encodedSize = std::max(sizeof(TType), sizeof(Uint32));
		constexpr size_t encodedValuesNum = encodedSize / sizeof(Uint32);

		SPT_CHECK(!Ended() && (m_bufferSize - m_currentOffset) >= encodedValuesNum);

		Uint32 encoded[encodedValuesNum];

		for(size_t i = 0; i < encodedValuesNum; ++i)
		{
			encoded[i] = m_buffer[static_cast<Uint64>(m_currentOffset++)];
		}

		return *reinterpret_cast<TType*>(encoded);
	}

	DebugArgument ReadArgument()
	{
		const Uint32 typeCode = Read<Uint32>();

		switch(typeCode)
		{
		case EArgumentTypeCode::Float:
			return Read<Real32>();
		case EArgumentTypeCode::Float2:
			return Read<math::Vector2f>();
		case EArgumentTypeCode::Float3:
			return Read<math::Vector3f>();
		case EArgumentTypeCode::Float4:
			return Read<math::Vector4f>();

		case EArgumentTypeCode::Uint:
			return Read<Uint32>();
		case EArgumentTypeCode::Uint2:
			return Read<math::Vector2u>();
		case EArgumentTypeCode::Uint3:
			return Read<math::Vector3u>();
		case EArgumentTypeCode::Uint4:
			return Read<math::Vector4u>();

		case EArgumentTypeCode::Int:
			return Read<Int32>();
		case EArgumentTypeCode::Int2:
			return Read<math::Vector2i>();
		case EArgumentTypeCode::Int3:
			return Read<math::Vector3i>();
		case EArgumentTypeCode::Int4:
			return Read<math::Vector4i>();

		default:
			return InvalidArgument{};
		}
	}

	Bool Ended() const
	{
		return m_currentOffset >= m_bufferSize;
	}

private:

	rhi::RHIMappedBuffer<Uint32> m_buffer;

	Uint32 m_currentOffset;
	Uint32 m_bufferSize;
};

} // spt::gfx::dbg