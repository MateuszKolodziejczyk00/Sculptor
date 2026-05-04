#pragma once

#include "Material.h"
#include "MaterialFeatures.h"
#include "RenderSceneRegistry.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "Containers/PagedGenerationalPool.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(MaterialPBRData)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>, baseColorTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, metallicRoughnessTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, normalsTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>,         alphaTexture)
	SHADER_STRUCT_FIELD(mat::EmissiveMaterialData,              emissiveData)
	SHADER_STRUCT_FIELD(mat::MaterialDepthData,                 depthData)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>,         occlusionTexture)
	SHADER_STRUCT_FIELD(Real32,                                 metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f,                         baseColorFactor)
	SHADER_STRUCT_FIELD(Real32,                                 roughnessFactor)
END_SHADER_STRUCT();


using MaterialSlotsChunkHandle = lib::PagedGenerationalPoolHandle<struct MaterialSlotsChunk>;


struct MaterialSlotsChunk
{
	lib::StaticArray<ecs::EntityHandle, 8u> slots;
	MaterialSlotsChunkHandle next;
};


struct SceneMaterials
{
	SceneMaterials()
		: slots("SceneMaterials_MaterialSlotsPool")
	{
	}

	lib::PagedGenerationalPool<MaterialSlotsChunk> slots;

	MaterialSlotsChunkHandle CreateMaterialSlotsChain(lib::Span<const ecs::EntityHandle> slotsArray)
	{
		if (slotsArray.empty())
		{
			return MaterialSlotsChunkHandle{};
		}

		Uint32 createdSlotsNum = 0u;
		MaterialSlotsChunkHandle firstChunkHandle;

		MaterialSlotsChunk* lastChunk = nullptr;

		while (createdSlotsNum < slotsArray.size())
		{
			const lib::Span<const ecs::EntityHandle> chunkSlots = slotsArray.subspan(createdSlotsNum, std::min<Uint32>(8u, static_cast<Uint32>(slotsArray.size() - createdSlotsNum)));

			MaterialSlotsChunk chunk;
			for (SizeType idx = 0; idx < chunkSlots.size(); ++idx)
			{
				chunk.slots[idx] = chunkSlots[idx];
			}

			const MaterialSlotsChunkHandle newChunkHandle = slots.Add(chunk);

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

	void DestroyMaterialSlotsChain(MaterialSlotsChunkHandle firstChunkHandle)
	{
		MaterialSlotsChunkHandle currentHandle = firstChunkHandle;
		const MaterialSlotsChunk* chunk = slots.Get(firstChunkHandle);
		while (chunk)
		{
			const MaterialSlotsChunkHandle nextHandle = chunk->next;
			slots.Delete(currentHandle);

			currentHandle = nextHandle;
			chunk = slots.Get(currentHandle);
		}
	}

	void CreateMaterialSlot(ecs::EntityHandle slot)
	{
		MaterialSlotsChunk chunk;
		chunk.slots[0] = slot;

		slots.Add(chunk);
	}
};

} // spt::rsc
