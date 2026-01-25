#ifndef BINDLESS_TYPES_HLSLI
#define BINDLESS_TYPES_HLSLI


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


template<typename T>
struct NamedBufferDescriptor
{
	uint idx;

	GPUPtr<T> GetElemPtr(in uint dataIdx)
	{
		GPUPtr<T> ptr;
		ptr.descriptorIdx = idx;
		ptr.dataIdx       = dataIdx;
		return ptr;
	}
};


template<typename T>
struct SRVTexture3D
{
	uint descriptorIdx;
	uint metaData;

	Texture3D<T> GetResource()
	{
		return ResourceDescriptorHeap[descriptorIdx];
	}

	T Load(in int3 coords)
	{
		return Load(int4(coords, 0));
	}

	T Load(in int4 coords)
	{
		const Texture3D<T> texture = GetResource();
		return texture.Load(coords);
	}

	T SampleLevel(in SamplerState s, in float3 uvw, in float level = 0.f)
	{
		const Texture3D<T> texture = GetResource();
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

	Texture2D<T> GetResource()
	{
		return ResourceDescriptorHeap[descriptorIdx];
	}

	T Load(in int2 coords)
	{
		return Load(int3(coords, 0));
	}

	T Load(in int3 coords)
	{
		const Texture2D<T> texture = GetResource();
		return texture.Load(coords);
	}

	T SampleLevel(in SamplerState s, in float2 uv, in float level = 0.f)
	{
		const Texture2D<T> texture = GetResource();
		return texture.SampleLevel(s, uv, level);
	}

	T SampleGrad(in SamplerState s, in float2 uv, in float2 duv_dx, in float2 duv_dy)
	{
		const Texture2D<T> texture = GetResource();
		return texture.SampleGrad(s, uv, duv_dx, duv_dy);
	}

	T Sample(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return texture.Sample(s, uv);
	}

	float SampleCmp(in SamplerComparisonState s, in float2 uv, in T cmp)
	{
		const Texture2D<T> texture = GetResource();
		return texture.SampleCmp(s, uv, cmp);
	}

	template<typename TReturn>
	TReturn Gather(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return TReturn(texture.Gather(s, uv));
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

	RWTexture2D<T> GetResource()
	{
		return ResourceDescriptorHeap[descriptorIdx];
	}

	T Load(in int2 coords)
	{
		RWTexture2D<T> texture = GetResource();
		return texture[coords];
	}

	void Store(in int2 coords, in T value)
	{
		RWTexture2D<T> texture = GetResource();
		texture[coords] = value;
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	uint2 GetResolution()
	{
		RWTexture2D<T> texture = GetResource();
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

	T AtomicAdd(in uint idx, in T value)
	{
		RWStructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		T original;
		InterlockedAdd(buffer[idx], value, OUT original);
		return original;
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

	GPUPtr<T> PtrAt(in uint idx)
	{
		GPUPtr<T> ptr;
		ptr.descriptorIdx = descriptorIdx;
		ptr.dataIdx       = idx;
		return ptr;
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


struct TLAS
{
	uint descriptorIdx;
	uint metaData;

	RaytracingAccelerationStructure GetResource()
	{
		return ResourceDescriptorHeap[descriptorIdx];
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
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


#define SPT_LINEAR_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX     0
#define SPT_NEAREST_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX    1
#define SPT_LINEAR_REPEAT_SAMPLER_DESCRIPTOR_IDX         2
#define SPT_NEAREST_REPEAT_SAMPLER_DESCRIPTOR_IDX        3
#define SPT_LINEAR_MIN_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX 4
#define SPT_LINEAR_MAX_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX 5
#define SPT_MATERIAL_ANISO_SAMPLER_DESCRIPTOR_IDX        6
#define SPT_MATERIAL_LINEAR_SAMPLER_DESCRIPTOR_IDX       7


struct BindlessSamplers
{
	static SamplerState LinearClampEdge()
	{
		return SamplerDescriptorHeap[SPT_LINEAR_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
	}
	
	static SamplerState NearestClampEdge()
	{
		return SamplerDescriptorHeap[SPT_NEAREST_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
	}
	
	static SamplerState LinearRepeat()
	{
		return SamplerDescriptorHeap[SPT_LINEAR_REPEAT_SAMPLER_DESCRIPTOR_IDX];
	}
	
	static SamplerState NearestRepeat()
	{
		return SamplerDescriptorHeap[SPT_NEAREST_REPEAT_SAMPLER_DESCRIPTOR_IDX];
	}
	
	static SamplerState LinearMinClampEdge()
	{
		return SamplerDescriptorHeap[SPT_LINEAR_MIN_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
	}
	
	static SamplerState LinearMaxClampEdge()
	{
		return SamplerDescriptorHeap[SPT_LINEAR_MAX_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
	}

	static SamplerState MaterialAniso()
	{
		return SamplerDescriptorHeap[SPT_MATERIAL_ANISO_SAMPLER_DESCRIPTOR_IDX];
	}

	static SamplerState MaterialLinear()
	{
		return SamplerDescriptorHeap[SPT_MATERIAL_LINEAR_SAMPLER_DESCRIPTOR_IDX];
	}
};

#endif // BINDLESS_TYPES_HLSLI
