#ifndef BINDLESS_TYPES_HLSLI
#define BINDLESS_TYPES_HLSLI


struct NamedBufferDescriptorIdx
{
	uint idx;
};


template<typename T>
struct SRVTexture3D
{
	uint descriptorIdx;
	uint metaData;

	T Load(in int3 coords)
	{
		return Load(int4(coords, 0));
	}

	T Load(in int4 coords)
	{
		const Texture3D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture.Load(coords);
	}

	T SampleLevel(in SamplerState s, in float3 uvw, in float level = 0.f)
	{
		const Texture3D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture.Sample(s, uvw, level);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


template<typename T>
struct SRVTexture2D
{
	uint descriptorIdx;
	uint metaData;

	T Load(in int2 coords)
	{
		return Load(int3(coords, 0));
	}

	T Load(in int3 coords)
	{
		const Texture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture.Load(coords);
	}

	T SampleLevel(in SamplerState s, in float2 uv, in float level = 0.f)
	{
		const Texture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture.Sample(s, uv, level);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


template<typename T>
struct UAVTexture2D
{
	uint descriptorIdx;
	uint metaData;

	T Load(in int2 coords)
	{
		RWTexture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture[coords];
	}

	void Store(in int2 coords, in T value)
	{
		RWTexture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		texture[coords] = value;
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	uint2 GetResolution()
	{
		RWTexture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		uint2 resolution;
		texture.GetDimensions(resolution.x, resolution.y);
		return resolution;
	}
};


template<typename T>
struct RWTypedBuffer
{
	uint descriptorIdx;
	uint metaData;

	T Load(in uint idx)
	{
		RWStructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer[idx];
	}

	void Store(in uint idx, in T data)
	{
		RWStructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		buffer[idx] = data;
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


template<typename T>
struct TypedBuffer
{
	uint descriptorIdx;
	uint metaData;

	T Load(in uint idx)
	{
		StructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer[idx];
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


struct RWByteBuffer
{
	uint descriptorIdx;
	uint metaData;

	template<typename T>
	T Load(in uint offset)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load < T > (offset);
	}

	template<typename T>
	void Store(in uint offset, in T data)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		buffer.Store < T > (offset, data);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


struct ByteBuffer
{
	uint descriptorIdx;
	uint metaData;

	template<typename T>
	T Load(in uint offset)
	{
		ByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load < T > (offset);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};

template<typename TDataType>
struct GPUPtr
{
	uint descriptorIdx;
	uint dataIdx;

	TDataType Load()
	{
		StructuredBuffer<TDataType> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer[dataIdx];
	}
};


template<typename TNamedBuffer, typename TDataType>
struct GPUNamedElemPtr
{
	uint dataIdx;

	TDataType Load(in uint offset = 0u)
	{
		const uint descriptorIdx = TNamedBuffer::Get();
		StructuredBuffer<TDataType> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer[dataIdx + offset];
	}

	TDataType operator[](in uint idx)
	{
		return Load(idx);
	}

	GPUPtr<TDataType> AsGenericPtr()
	{
		const uint descriptorIdx = TNamedBuffer::Get();
		GPUPtr<TDataType> ptr;
		ptr.descriptorIdx = descriptorIdx;
		ptr.dataIdx       = dataIdx;
		return ptr;
	}
};


template<typename TNamedBuffer, typename TDataType>
struct GPUNamedElemsSpan
{
	GPUNamedElemPtr<TNamedBuffer, TDataType> begin;
	uint size;

	TDataType operator[](in uint idx)
	{
		return begin.Load(idx);
	}

	
	GPUNamedElemPtr<TNamedBuffer, TDataType> GetElemPtr(in uint idx)
	{
		GPUNamedElemPtr<TNamedBuffer, TDataType> ptr = begin;
		ptr.dataIdx += idx;
		return ptr;
	}

	uint GetSize()
	{
		return size;
	}
};

#endif // BINDLESS_TYPES_HLSLI