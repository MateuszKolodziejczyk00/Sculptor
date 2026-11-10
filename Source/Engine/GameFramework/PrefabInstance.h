#pragma once

#include "SculptorCoreTypes.h"
#include "PrefabAsset.h"
#include "GameFrameworkTypes.h"
#include "Containers/PagedGenerationalPool.h"
#include "MeshPrefab.h"
#include "LightPrefab.h"


namespace spt::gf
{

struct PrefabInstance;


using EntitiesChunkHandle = lib::PagedGenerationalPoolHandle<struct EntitiesChunk>;


struct GenericEntityHandle
{
	Uint32 idx = 0xFFFFFFFFu;
	Uint16 generation  = 0u;

	template<typename THandleType>
	THandleType ToTypedHandle() const
	{
		return THandleType{ .idx = idx, .generation = generation };
	}
};


struct EntitiesChunk
{
	static constexpr Uint32 MaxEntitiesNum = 4u;
	lib::StaticArray<GenericEntityHandle, MaxEntitiesNum> entities;
	EntitiesChunkHandle next;
};


struct WorldEntities
{
	WorldEntities()
		: chunks("World_EntitiesChunksPool")
	{
	}

	lib::PagedGenerationalPool<EntitiesChunk> chunks;

	EntitiesChunkHandle CreateEntitiesChunksList(Uint32 entitiesNum)
	{
		EntitiesChunkHandle firstChunkHandle = chunks.Add();
		EntitiesChunk* currentChunk = &chunks.GetRef(firstChunkHandle);

		constexpr Uint32 entitiesPerChunk = EntitiesChunk::MaxEntitiesNum;

		for (Uint32 entityIdx = entitiesPerChunk; entityIdx < entitiesNum; entityIdx += entitiesPerChunk)
		{
			currentChunk->next = chunks.Add();
			currentChunk = &chunks.GetRef(currentChunk->next);
		}

		return firstChunkHandle;
	}

	void DestroyEntitiesChunksList(EntitiesChunkHandle firstChunkHandle)
	{
		EntitiesChunkHandle currentChunkHandle = firstChunkHandle;
		while (currentChunkHandle.IsValid())
		{
			const EntitiesChunkHandle nextChunkHandle = chunks.GetRef(currentChunkHandle).next;
			chunks.Delete(currentChunkHandle);
			currentChunkHandle = nextChunkHandle;
		}
	}

	void SetEntity(EntitiesChunkHandle firstChunkHandle, Uint32 entityIdx, GenericEntityHandle entityHandle)
	{
		EntitiesChunkHandle currentChunkHandle = firstChunkHandle;
		for (Uint32 chunkIdx = 0u; chunkIdx < entityIdx / EntitiesChunk::MaxEntitiesNum; ++chunkIdx)
		{
			currentChunkHandle = chunks.GetRef(currentChunkHandle).next;
		}

		EntitiesChunk& currentChunk = chunks.GetRef(currentChunkHandle);
		currentChunk.entities[entityIdx % EntitiesChunk::MaxEntitiesNum] = entityHandle;
	}

	template<typename THandleType>
	void SetTypedEntity(EntitiesChunkHandle firstChunkHandle, Uint32 entityIdx, THandleType entityHandle)
	{
		SetEntity(firstChunkHandle, entityIdx, GenericEntityHandle{ .idx = entityHandle.idx, .generation = entityHandle.generation });
	}

	GenericEntityHandle GetEntityHandle(EntitiesChunkHandle firstChunkHandle, Uint32 entityIdx) const
	{
		EntitiesChunkHandle currentChunkHandle = firstChunkHandle;
		for (Uint32 chunkIdx = 0u; chunkIdx < entityIdx / EntitiesChunk::MaxEntitiesNum; ++chunkIdx)
		{
			currentChunkHandle = chunks.GetRef(currentChunkHandle).next;
		}

		const EntitiesChunk& currentChunk = chunks.GetRef(currentChunkHandle);
		return currentChunk.entities[entityIdx % EntitiesChunk::MaxEntitiesNum];
	}

	template<typename THandleType>
	THandleType GetTypedEntityHandle(EntitiesChunkHandle firstChunkHandle, Uint32 entityIdx) const
	{
		return GetEntityHandle(firstChunkHandle, entityIdx).ToTypedHandle<THandleType>();
	}
};


using PrefabInstanceHandle = lib::PagedGenerationalPoolHandle<PrefabInstance>;


struct PrefabInstance
{
	Transform transform;

	as::PrefabAssetHandle prefab;

	MeshEntityHandle  meshes;
	LightEntityHandle lights;

	EntitiesChunkHandle entities;

	PrefabInstanceHandle child;
	PrefabInstanceHandle next;
};


struct WorldPrefabs
{
	WorldPrefabs()
		: instances("World_PrefabInstancesPool")
	{
	}

	lib::PagedGenerationalPool<PrefabInstance> instances;
};


} // spt::gf
