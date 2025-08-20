#ifndef BINDLESS_TYPES_HLSLI
#define BINDLESS_TYPES_HLSLI


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


#endif // BINDLESS_TYPES_HLSLI