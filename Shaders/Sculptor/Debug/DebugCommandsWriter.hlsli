#ifndef DEBUG_COMMANDS_WRITER_HLSLI
#define DEBUG_COMMANDS_WRITER_HLSLI

#if SPT_META_PARAM_DEBUG_FEATURES

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


interface IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer);
}

extension float : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(FLOAT_TYPE_CODE);
		writer.Write(this);
	}
}

extension float2 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(FLOAT2_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
	}
}

extension float3 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(FLOAT3_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
	}
}

extension float4 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(FLOAT4_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
		writer.Write(this.w);
	}
}

extension uint : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(UINT_TYPE_CODE);
		writer.Write(this);
	}
}

extension uint2 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(UINT2_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
	}
}

extension uint3 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(UINT3_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
	}
}

extension uint4 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(UINT4_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
		writer.Write(this.w);
	}
}

extension int : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(INT_TYPE_CODE);
		writer.Write(this);
	}
}

extension int2 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(INT2_TYPE_CODE); // Fixed original typo: was UINT2_TYPE_CODE
		writer.Write(this.x);
		writer.Write(this.y);
	}
}

extension int3 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(INT3_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
	}
}

extension int4 : IDebugStreamDataType
{
	void writeSelfWithCode(inout DebugCommandsWriter writer)
	{
		writer.Write(INT4_TYPE_CODE);
		writer.Write(this.x);
		writer.Write(this.y);
		writer.Write(this.z);
		writer.Write(this.w);
	}
}


struct DebugCommandsWriter
{
	[mutating] void Write(uint val)
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

	[mutating] void Write(int val)
	{
		Write(uint(val));
	}

	[mutating] void Write(float val)
	{
		Write(asuint(val));
	}

	[mutating] void WriteLiteral(Literal literal)
	{
		Write(literal.val.x);
		Write(literal.val.y);
	}

	// Write with code

	[mutating] void WriteSingleWithCode<T : IDebugStreamDataType>(T val)
	{
		val.writeSelfWithCode(this);
	}

	[mutating] void WriteWithCodes()
	{
	}

	[mutating] void WriteWithCodes<TType1 : IDebugStreamDataType>(TType1 val1)
	{
		WriteSingleWithCode(val1);
	}

	[mutating] void WriteWithCodes<TType1 : IDebugStreamDataType, TType2 : IDebugStreamDataType>(TType1 val1, TType2 val2)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
	}

	[mutating] void WriteWithCodes<TType1 : IDebugStreamDataType, TType2 : IDebugStreamDataType, TType3 : IDebugStreamDataType>(TType1 val1, TType2 val2, TType3 val3)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
		WriteSingleWithCode(val3);
	}

	[mutating] void WriteWithCodes<TType1 : IDebugStreamDataType, TType2 : IDebugStreamDataType, TType3 : IDebugStreamDataType, TType4 : IDebugStreamDataType>(TType1 val1, TType2 val2, TType3 val3, TType4 val4)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
		WriteSingleWithCode(val3);
		WriteSingleWithCode(val4);
	}

	[mutating] void WriteWithCodes<TType1 : IDebugStreamDataType, TType2 : IDebugStreamDataType, TType3 : IDebugStreamDataType, TType4 : IDebugStreamDataType, TType5 : IDebugStreamDataType>(TType1 val1, TType2 val2, TType3 val3, TType4 val4, TType5 val5)
	{
		WriteSingleWithCode(val1);
		WriteSingleWithCode(val2);
		WriteSingleWithCode(val3);
		WriteSingleWithCode(val4);
		WriteSingleWithCode(val5);
	}

	void Flush()
	{
		if(m_hasValidSequence)
		{
			const uint size = m_currentIdx;

			uint offset = PARAM_ShaderDebugCommandBufferParams->debugCommandsBufferOffset.AtomicAdd(0u, size);

			if(offset + size < PARAM_ShaderDebugCommandBufferParams->bufferSize)
			{
				for (uint i = 0; i < size; ++i)
				{
					PARAM_ShaderDebugCommandBufferParams->debugCommandsBuffer[offset + i] = m_buffer[i];
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

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // DEBUG_COMMANDS_WRITER_HLSLI
