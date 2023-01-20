#pragma once 

#include "SculptorCoreTypes.h"
#include "GeometryManager.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "RenderingDataRegistry.h"


namespace spt::rsc
{

struct SubmeshBuildData
{
	SubmeshBuildData()
		: vertexCount(0)
	{ }

	SubmeshGPUData submesh;
	Uint32 vertexCount;
};


class MeshBuilder abstract
{
public:

	MeshBuilder();

	RenderingDataEntityHandle EmitMeshGeometry();

protected:

	void BeginNewSubmesh();
	SubmeshBuildData& GetBuiltSubmesh();

	Uint64 GetCurrentDataSize() const;

	template<typename TDestType, typename TSourceType>
	SizeType AppendData(const unsigned char* data, SizeType componentsNum, SizeType stride, SizeType count, const lib::DynamicArray<SizeType>& componentsRemapping = lib::DynamicArray<SizeType>{});

private:
	
#if SPT_DEBUG
	void ValidateSubmesh(const SubmeshBuildData& submeshBuildData);
#endif // SPT_DEBUG

	void OptimizeSubmesh(SubmeshBuildData& submeshBuildData);
	void BuildMeshlets(SubmeshBuildData& submeshBuildData);

	Byte* AppendData(Uint64 appendSize);

	lib::DynamicArray<Byte> m_geometryData;
	
	lib::DynamicArray<SubmeshBuildData> m_submeshes;
	lib::DynamicArray<MeshletGPUData> m_meshlets;
};


template<typename TDestType, typename TSourceType>
SizeType MeshBuilder::AppendData(const unsigned char* sourceData, SizeType componentsNum, SizeType stride, SizeType count, const lib::DynamicArray<SizeType>& componentsRemapping /*= lib::DynamicArray<SizeType>{}*/)
{
	const SizeType destDataSize = count * componentsNum * sizeof(TDestType);
	Byte* destPtr = AppendData(destDataSize);

	const SizeType offset = destPtr - m_geometryData.data();

	if (componentsRemapping.empty())
	{
		if constexpr (std::is_same_v<TDestType, TSourceType>)
		{
			if (stride == sizeof(TDestType) * componentsNum)
			{
				memcpy(destPtr, sourceData, count * componentsNum * sizeof(TDestType));
				return offset;
			}
		}
	}

	SPT_CHECK(componentsRemapping.size() == componentsNum || componentsRemapping.empty());
	lib::DynamicArray<SizeType> remapping(componentsNum);
	for (SizeType idx = 0; idx < componentsNum; ++idx)
	{
		remapping[idx] = componentsRemapping.empty() ? idx : componentsRemapping[idx];
	}

	const SizeType destStride = sizeof(TDestType) * componentsNum;
	for (SizeType sourceOffset = 0, destOffset = 0; destOffset < destDataSize; sourceOffset += stride, destOffset += destStride)
	{
		TDestType* dest = reinterpret_cast<TDestType*>(&destPtr[destOffset]);
		const TSourceType* source = reinterpret_cast<const TSourceType*>(&sourceData[sourceOffset]);
		for (SizeType componentIdx = 0; componentIdx < componentsNum; ++componentIdx)
		{
			dest[componentIdx] = static_cast<TDestType>(source[remapping[componentIdx]]);
		}
	}

	return offset;
}

} // spt::rsc
