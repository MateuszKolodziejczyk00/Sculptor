#pragma once 

#include "SculptorCoreTypes.h"
#include "GeometryManager.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "ECSRegistry.h"
#include "StaticMeshes/RenderMesh.h"


namespace spt::rdr
{
class BLASBuilder;
} // spt::rdr


namespace spt::rsc
{

struct MeshBuildParameters
{
	MeshBuildParameters()
		: optimizeMesh(true)
		, blasBuilder(nullptr)
	{ }

	Bool optimizeMesh;

	rdr::BLASBuilder* blasBuilder;
};


class MeshBuilder abstract
{
public:

	explicit MeshBuilder(const MeshBuildParameters& parameters);

	lib::MTHandle<RenderMesh> EmitMeshGeometry();

	void Build();

	MeshDefinition CreateMeshDefinition() const;

protected:

	const MeshBuildParameters& GetParameters() const;

	void BeginNewSubmesh();
	SubmeshDefinition& GetCurrentSubmesh();

	Uint64 GetCurrentDataSize() const;

	template<typename TDestType, typename TSourceType>
	SizeType AppendData(const unsigned char* data, SizeType componentsNum, SizeType stride, SizeType count);

private:
	
	template<typename TType>
	const TType* GetGeometryData(Uint32 offset) const
	{
		return reinterpret_cast<const TType*>(&m_geometryData[offset]);
	}

	template<typename TType>
	TType* GetGeometryDataMutable(Uint32 offset)
	{
		return reinterpret_cast<TType*>(&m_geometryData[offset]);
	}
	
#if SPT_DEBUG
	void ValidateSubmesh(SubmeshDefinition& submesh) const;
#endif // SPT_DEBUG

	void ComputeBoundingSphere(SubmeshDefinition& submesh) const;

	void OptimizeSubmesh(SubmeshDefinition& submesh);
	void BuildMeshlets(SubmeshDefinition& submesh);

	void BuildMeshletsCullingData(SubmeshDefinition& submesh);

	void ComputeMeshBoundingSphere();

	Byte* AppendData(Uint64 appendSize);

	lib::DynamicArray<Byte> m_geometryData;
	
	lib::DynamicArray<SubmeshDefinition> m_submeshes;
	lib::DynamicArray<MeshletDefinition> m_meshlets;

	math::Vector3f m_boundingSphereCenter;
	Real32 m_boundingSphereRadius;

	MeshBuildParameters m_parameters;
};


template<typename TDestType, typename TSourceType>
SizeType MeshBuilder::AppendData(const unsigned char* sourceData, SizeType componentsNum, SizeType stride, SizeType count)
{
	const SizeType destDataSize = count * componentsNum * sizeof(TDestType);
	Byte* destPtr = AppendData(destDataSize);

	const SizeType offset = destPtr - m_geometryData.data();

	if constexpr (std::is_same_v<TDestType, TSourceType>)
	{
		if (stride == sizeof(TDestType) * componentsNum)
		{
			memcpy(destPtr, sourceData, count * componentsNum * sizeof(TDestType));
			return offset;
		}
	}

	const SizeType destStride = sizeof(TDestType) * componentsNum;
	for (SizeType sourceOffset = 0, destOffset = 0; destOffset < destDataSize; sourceOffset += stride, destOffset += destStride)
	{
		TDestType* dest = reinterpret_cast<TDestType*>(&destPtr[destOffset]);
		const TSourceType* source = reinterpret_cast<const TSourceType*>(&sourceData[sourceOffset]);
		for (SizeType componentIdx = 0; componentIdx < componentsNum; ++componentIdx)
		{
			dest[componentIdx] = static_cast<TDestType>(source[componentIdx]);
		}
	}

	return offset;
}

} // spt::rsc
