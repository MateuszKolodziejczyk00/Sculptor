#ifndef BINDLESS_TYPES_HLSLI
#define BINDLESS_TYPES_HLSLI


struct GPUPtr<TDataType>
{
	uint descriptorIdx;
	uint dataIdx;

	TDataType Load()
	{
		ByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load<TDataType>(dataIdx * sizeof(TDataType));
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	uint GetIndex()
	{
		return dataIdx;
	}
};


interface INamedBuffer
{
	static uint Get();
};


struct NamedBufferDescriptor<T>
{
	uint idx;

	uint GetIndex()
	{
		return idx;
	}

	GPUPtr<T> GetElemPtr(in uint dataIdx)
	{
		GPUPtr<T> ptr;
		ptr.descriptorIdx = idx;
		ptr.dataIdx       = dataIdx;
		return ptr;
	}
};


struct SRVTexture3D<T : ITexelElement>
{
	uint descriptorIdx;
	uint metaData;

	Texture3D<T> GetResource()
	{
		Texture3D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture;
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
		return texture.SampleLevel(s, uvw, level);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	__subscript(int3 idx) -> T
	{
		get
		{
			return Load(idx);
		}
	}
};


struct SRVTexture2D<T : ITexelElement>
{
	static SRVTexture2D<T> Invalid()
	{
		SRVTexture2D<T> invalid;
		invalid.descriptorIdx = IDX_NONE_32;
		return invalid;
	}

	uint descriptorIdx;
	uint metaData;

	Texture2D<T> GetResource()
	{
		Texture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture;
	}

	T Load(in int coords)
	{
		return Load(int3(coords, 0, 0));
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

	float SampleCmp(in SamplerComparisonState s, in float2 uv, in float cmp)
	{
		const Texture2D<T> texture = GetResource();
		return texture.SampleCmp(s, uv, cmp);
	}

	vector<T.Element, 4> GatherRed(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return texture.GatherRed(s, uv);
	}

	vector<T.Element, 4> GatherGreen(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return texture.GatherGreen(s, uv);
	}

	vector<T.Element, 4> GatherBlue(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return texture.GatherBlue(s, uv);
	}

	vector<T.Element, 4> Gather(in SamplerState s, in float2 uv)
	{
		const Texture2D<T> texture = GetResource();
		return texture.Gather(s, uv);
	}

	uint2 GetResolution()
	{
		Texture2D<T> texture = GetResource();
		uint2 resolution;
		texture.GetDimensions(resolution.x, resolution.y);
		return resolution;
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	__subscript(int2 idx) -> T
	{
		get
		{
			return Load(idx);
		}
	}
};


struct UAVTexture2D<T : ITexelElement>
{
	uint descriptorIdx;
	uint metaData;

	RWTexture2D<T> GetResource()
	{
		RWTexture2D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture;
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

	__subscript(int2 idx) -> T
	{
		get
		{
			return Load(idx);
		}

		set
		{
			Store(idx, newValue);
		}
	}
};


struct UAVTexture3D<T : ITexelElement>
{
	uint descriptorIdx;
	uint metaData;

	RWTexture3D<T> GetResource()
	{
		RWTexture3D<T> texture = ResourceDescriptorHeap[descriptorIdx];
		return texture;
	}

	T Load(in int3 coords)
	{
		RWTexture3D<T> texture = GetResource();
		return texture[coords];
	}

	void Store(in int3 coords, in T value)
	{
		RWTexture3D<T> texture = GetResource();
		texture[coords] = value;
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}

	__subscript(int3 idx) -> T
	{
		get
		{
			return Load(idx);
		}

		set
		{
			Store(idx, newValue);
		}
	}
};


struct RWTypedBuffer<T>
{
	uint descriptorIdx;
	uint metaData;

	RWStructuredBuffer<T> GetResource()
	{
		RWStructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer;
	}

	T Load(in uint idx)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load<T>(idx * sizeof(T));
	}

	void Store(in uint idx, in T data)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		buffer.Store<T>(idx * sizeof(T), data);
	}

	RWTypedBuffer<U> Cast<U>()
	{
		RWTypedBuffer<U> buffer;
		buffer.descriptorIdx = descriptorIdx;
		buffer.metaData      = metaData;
		return buffer;
	}

	__subscript(uint idx) -> T
	{
		get
		{
			return Load(idx);
		}

		set
		{
			Store(idx, newValue);
		}
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


extension RWTypedBuffer<uint>
{
	uint AtomicAdd(in uint idx, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedAdd(idx * sizeof(uint), value, original);
		return original;
	}

	uint AtomicMin(in uint idx, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedMin(idx * sizeof(uint), value, original);
		return original;
	}

	uint AtomicMax(in uint idx, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedMax(idx * sizeof(uint), value, original);
		return original;
	}

	uint AtomicAnd(in uint idx, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedAnd(idx * sizeof(uint), value, original);
		return original;
	}

	uint AtomicOr(in uint idx, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedOr(idx * sizeof(uint), value, original);
		return original;
	}

	uint InterlockedCompareExchange(in uint idx, in uint compareValue, in uint value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint original;
		buffer.InterlockedCompareExchange(idx * sizeof(uint), compareValue, value, original);
		return original;
	}
};

extension RWTypedBuffer<uint64_t>
{
	uint64_t InterlockedCompareExchange(in uint idx, in uint64_t compareValue, in uint64_t value)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		uint64_t original;
		buffer.InterlockedCompareExchange64(idx * sizeof(uint64_t), compareValue, value, original);
		return original;
	}
};


struct TypedBuffer<T>
{
	uint descriptorIdx;
	uint metaData;

	StructuredBuffer<T> GetResource()
	{
		StructuredBuffer<T> buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer;
	}

	T Load(in uint idx)
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load<T>(idx * sizeof(T));
	}

	GPUPtr<T> PtrAt(in uint idx)
	{
		GPUPtr<T> ptr;
		ptr.descriptorIdx = descriptorIdx;
		ptr.dataIdx       = idx;
		return ptr;
	}

	__subscript(uint idx) -> T
	{
		get
		{
			return Load(idx);
		}
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

	RWByteAddressBuffer GetResource()
	{
		RWByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer;
	}

	T Load<T>(in uint offset)
	{
		RWByteAddressBuffer buffer = GetResource();
		return buffer.Load<T>(offset);
	}

	void Store<T>(in uint offset, in T data)
	{
		RWByteAddressBuffer buffer = GetResource();
		buffer.Store<T>(offset, data);
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

	T Load<T>(in uint offset)
	{
		ByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load<T>(offset);
	}

	bool IsValid()
	{
		return descriptorIdx != IDX_NONE_32;
	}
};


struct TLAS
{
	uint64_t address;

	RaytracingAccelerationStructure GetResource()
	{
		RaytracingAccelerationStructure as = {address};
		return as;
	}

	bool IsValid()
	{
		return address != 0u;
	}
};


struct GPUNamedElemPtr<TNamedBuffer : INamedBuffer, TDataType>
{
	uint dataIdx;

	TDataType Load(in uint offset = 0u)
	{
		const uint descriptorIdx = TNamedBuffer.Get();
		ByteAddressBuffer buffer = ResourceDescriptorHeap[descriptorIdx];
		return buffer.Load<TDataType>((dataIdx + offset) * sizeof(TDataType));
	}

	__subscript(uint idx) -> TDataType
	{
		get
		{
			return Load(idx);
		}
	}

	bool IsValid()
	{
		return dataIdx != IDX_NONE_32;
	}

	uint GetIndex()
	{
		return dataIdx;
	}

	GPUPtr<TDataType> AsGenericPtr()
	{
		const uint descriptorIdx = TNamedBuffer.Get();
		GPUPtr<TDataType> ptr;
		ptr.descriptorIdx = descriptorIdx;
		ptr.dataIdx       = dataIdx;
		return ptr;
	}
};


struct GPUNamedElemsSpan<TNamedBuffer : INamedBuffer, TDataType>
{
	GPUNamedElemPtr<TNamedBuffer, TDataType> begin;
	uint size;

	__subscript(uint idx) -> TDataType
	{
		get
		{
			return begin.Load(idx);
		}
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
		SamplerState handle = SamplerDescriptorHeap[SPT_LINEAR_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
	
	static SamplerState NearestClampEdge()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_NEAREST_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
	
	static SamplerState LinearRepeat()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_LINEAR_REPEAT_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
	
	static SamplerState NearestRepeat()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_NEAREST_REPEAT_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
	
	static SamplerState LinearMinClampEdge()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_LINEAR_MIN_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
	
	static SamplerState LinearMaxClampEdge()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_LINEAR_MAX_CLAMP_EDGE_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}

	static SamplerState MaterialAniso()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_MATERIAL_ANISO_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}

	static SamplerState MaterialLinear()
	{
		SamplerState handle = SamplerDescriptorHeap[SPT_MATERIAL_LINEAR_SAMPLER_DESCRIPTOR_IDX];
		return handle;
	}
};

#endif // BINDLESS_TYPES_HLSLI
