#pragma once

#include "SculptorCoreTypes.h"
#include "Containers/PagedGenerationalPool.h"
#include "MaterialAsset.h"
#include "MathUtils.h"


namespace spt::gf
{


struct MaterialAssetSlot
{
	as::MaterialAssetHandle material;
};


using MaterialAssetSlotsChunkHandle = lib::PagedGenerationalPoolHandle<struct MaterialAssetSlotsChunk>;


struct MaterialAssetSlotsChunk
{
	static constexpr Uint32 MaxMaterialsNum = 1u;
	lib::StaticArray<MaterialAssetSlot, MaxMaterialsNum> slots;
	MaterialAssetSlotsChunkHandle next;
};


struct WorldMaterialSlots
{
	WorldMaterialSlots()
		: slots("World_MaterialSlotsPool")
	{
	}

	lib::PagedGenerationalPool<MaterialAssetSlotsChunk> slots;

	MaterialAssetSlotsChunkHandle CreateMaterialSlotsChain(lib::Span<as::MaterialAssetHandle> slotsArray)
	{
		if (slotsArray.empty())
		{
			return MaterialAssetSlotsChunkHandle{};
		}

		Uint32 createdSlotsNum = 0u;
		MaterialAssetSlotsChunkHandle firstChunkHandle;

		MaterialAssetSlotsChunk* lastChunk = nullptr;

		while (createdSlotsNum < slotsArray.size())
		{
			const lib::Span<const as::MaterialAssetHandle> chunkSlots = slotsArray.subspan(createdSlotsNum, std::min<Uint32>(MaterialAssetSlotsChunk::MaxMaterialsNum, static_cast<Uint32>(slotsArray.size() - createdSlotsNum)));

			MaterialAssetSlotsChunk chunk;
			for (SizeType idx = 0; idx < chunk.slots.size(); ++idx)
			{
				chunk.slots[idx].material = chunkSlots[idx];
			}

			const MaterialAssetSlotsChunkHandle newChunkHandle = slots.Add(chunk);

			if (lastChunk != nullptr)
			{
				lastChunk->next = newChunkHandle;
			}
			else
			{
				SPT_CHECK(!firstChunkHandle.IsValid());
				firstChunkHandle = newChunkHandle;
			}

			lastChunk = slots.Get(newChunkHandle);
			createdSlotsNum += static_cast<Uint32>(chunkSlots.size());
		}

		return firstChunkHandle;
	}

	void DestroyMaterialSlotsChain(MaterialAssetSlotsChunkHandle firstChunkHandle)
	{
		MaterialAssetSlotsChunkHandle currentHandle = firstChunkHandle;
		const MaterialAssetSlotsChunk* chunk = slots.Get(firstChunkHandle);
		while (chunk)
		{
			const MaterialAssetSlotsChunkHandle nextHandle = chunk->next;
			slots.Delete(currentHandle);

			currentHandle = nextHandle;
			chunk = slots.Get(currentHandle);
		}
	}

	void CreateMaterialSlot(as::MaterialAssetHandle slot)
	{
		MaterialAssetSlotsChunk chunk;
		chunk.slots[0].material = slot;

		slots.Add(chunk);
	}
};

} // spt::gf
