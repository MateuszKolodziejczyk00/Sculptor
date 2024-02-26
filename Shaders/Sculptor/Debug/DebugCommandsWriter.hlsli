#ifndef DEBUG_COMMANDS_WRITER_HLSLI
#define DEBUG_COMMANDS_WRITER_HLSLI

#if SPT_SHADERS_DEBUG_FEATURES

#include "Debug/DebugCommon.hlsli"

namespace debug
{

#define FLOAT_TYPE_CODE  uint(1)
#define FLOAT2_TYPE_CODE uint(2)
#define FLOAT3_TYPE_CODE uint(3)
#define FLOAT4_TYPE_CODE uint(4)

#define UINT_TYPE_CODE  uint(5)
#define UINT2_TYPE_CODE uint(6)
#define UINT3_TYPE_CODE uint(7)
#define UINT4_TYPE_CODE uint(8)

#define INT_TYPE_CODE  uint(9)
#define INT2_TYPE_CODE uint(10)
#define INT3_TYPE_CODE uint(11)
#define INT4_TYPE_CODE uint(12)


class DebugCommandsWriter
{
	void Write(uint val)
	{
		if(m_currentIdx < 255)
		{
			m_buffer[m_currentIdx++] = val;
		}
		else
		{
			m_hasValidSequence = false;
		}
	}

	void Write(int val)
	{
		Write(uint(val));
	}

	void Write(float val)
	{
		Write(asuint(val));
	}

	void WriteLiteral(Literal literal)
	{
		Write(literal.val.x);
		Write(literal.val.y);
	}

	// Write with code

	void WriteSingleWithCode(float val)
	{
		Write(FLOAT_TYPE_CODE);
		Write(val);
	}

	void WriteSingleWithCode(float2 val)
	{
		Write(FLOAT2_TYPE_CODE);
		Write(val.x);
		Write(val.y);
	}

	void WriteSingleWithCode(float3 val)
	{
		Write(FLOAT3_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
	}

	void WriteSingleWithCode(float4 val)
	{
		Write(FLOAT4_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
		Write(val.w);
	}

	void WriteSingleWithCode(uint val)
	{
		Write(UINT_TYPE_CODE);
		Write(val);
	}

	void WriteSingleWithCode(uint2 val)
	{
		Write(UINT2_TYPE_CODE);
		Write(val.x);
		Write(val.y);
	}

	void WriteSingleWithCode(uint3 val)
	{
		Write(UINT3_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
	}

	void WriteSingleWithCode(uint4 val)
	{
		Write(UINT4_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
		Write(val.w);
	}

	void WriteSingleWithCode(int val)
	{
		Write(INT_TYPE_CODE);
		Write(val);
	}

	void WriteSingleWithCode(int2 val)
	{
		Write(UINT2_TYPE_CODE);
		Write(val.x);
		Write(val.y);
	}

	void WriteSingleWithCode(int3 val)
	{
		Write(INT3_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
	}

	void WriteSingleWithCode(int4 val)
	{
		Write(INT4_TYPE_CODE);
		Write(val.x);
		Write(val.y);
		Write(val.z);
		Write(val.w);
	}

	void WriteWithCodes()
	{
	}

	template<typename TType1>
	void WriteWithCodes(TType1 val1)
	{
		WriteSingleWithCode(val1);
	}

	template<typename TType1, typename TType2>
	void WriteWithCodes(TType1 val1, TType2 val2)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
	}

	template<typename TType1, typename TType2, typename TType3>
	void WriteWithCodes(TType1 val1, TType2 val2, TType3 val3)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
		WriteSingleWithCode(val3);
	}

	template<typename TType1, typename TType2, typename TType3, typename TType4>
	void WriteWithCodes(TType1 val1, TType2 val2, TType3 val3, TType4 val4)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
		WriteSingleWithCode(val3);
		WriteSingleWithCode(val4);
	}

	void Flush()
	{
		if(m_hasValidSequence)
		{
			const uint size = m_currentIdx;

			uint offset = 0;
			InterlockedAdd(u_debugCommandsBufferOffset[0], size, OUT offset);

			if(offset + size < u_debugCommandsBufferParams.bufferSize)
			{
				for (uint i = 0; i < size; ++i)
				{
					u_debugCommandsBuffer[offset + i] = m_buffer[i];
				}
			}
		}
	}

	uint m_buffer[256];
	uint m_currentIdx;

	bool m_hasValidSequence;
};


DebugCommandsWriter CreateDebugCommandsWriter()
{
	DebugCommandsWriter result;
	result.m_currentIdx = 0;
	result.m_hasValidSequence = true;
	return result;
}

} // debug

#endif // SPT_SHADERS_DEBUG_FEATURES

#endif // DEBUG_COMMANDS_WRITER_HLSLI
