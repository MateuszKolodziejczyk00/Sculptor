#pragma once

#include "SculptorCoreTypes.h"
#include "PrefabAsset.h"
#include "GameFrameworkTypes.h"
#include "Containers/PagedGenerationalPool.h"
#include "MeshPrefab.h"


namespace spt::gf
{

struct PrefabInstance;


struct PrefabInstance
{
	Transform transform;

	as::PrefabAssetHandle prefab;

	MeshesChunkHandle meshesChunk;
};


using PrefabInstanceHandle = lib::PagedGenerationalPoolHandle<PrefabInstance>;


struct WorldPrefabs
{
	WorldPrefabs()
		: instances("World_PrefabInstancesPool")
	{
	}

	lib::PagedGenerationalPool<PrefabInstance> instances;
};


} // spt::gf
