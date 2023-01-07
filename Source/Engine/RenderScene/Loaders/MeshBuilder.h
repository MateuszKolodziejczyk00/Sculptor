#pragma once 

#include "SculptorCoreTypes.h"
#include "GeometryManager.h"
#include "StaticMeshes/StaticMeshGeometry.h"


namespace spt::rsc
{

class MeshBuilder abstract
{
public:

	MeshBuilder();

	StaticMeshGeometryData EmitMeshGeometry();

protected:

	void BeginNewPrimitive();
	PrimitiveGeometryInfo& GetBuiltPrimitive();

	Uint64 GetCurrentDataSize() const;

	template<typename TDestType, typename TSourceType>
	void AppendData(const unsigned char* data, SizeType componentsNum, SizeType stride, SizeType count);

private:

	Byte* AppendData(Uint64 appendSize);

	lib::DynamicArray<Byte> m_geometryData;
	lib::DynamicArray<PrimitiveGeometryInfo> m_primitives;
};


template<typename TDestType, typename TSourceType>
void MeshBuilder::AppendData(const unsigned char* sourceData, SizeType componentsNum, SizeType stride, SizeType count)
{
	const SizeType destDataSize = count * componentsNum * sizeof(TDestType);
	Byte* destPtr = AppendData(destDataSize);

	if constexpr (std::is_same_v<TDestType, TSourceType>)
	{
		if (stride == sizeof(TDestType) * componentsNum)
		{
			memcpy(destPtr, sourceData, count * componentsNum * sizeof(TDestType));
			return;
		}
	}

	for (SizeType sourceOffset = 0, destOffset = 0; destOffset < destDataSize; sourceOffset += stride, destOffset += sizeof(TDestType))
	{
		TDestType* dest = reinterpret_cast<TDestType*>(&destPtr[destOffset]);
		const TSourceType* source = reinterpret_cast<const TSourceType*>(&sourceData[sourceOffset]);
		for (SizeType componentIdx = 0; componentIdx < componentsNum; ++componentIdx)
		{
			dest[componentIdx] = static_cast<TDestType>(source[componentIdx]);
		}
	}
}

} // spt::rsc
