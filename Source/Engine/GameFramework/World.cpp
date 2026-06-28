#include "World.h"
#include "JobSystem.h"
#include "MeshPrefab.h"
#include "WorldMaterials.h"
#include "RenderScene.h"


SPT_DEFINE_LOG_CATEGORY(World, true);


namespace spt::gf
{

World::World()
	: prefabs(*(new WorldPrefabs()))
	, meshes(*(new WorldMeshes()))
	, materials(*(new WorldMaterialSlots()))
	, m_renderScene(lib::MakeShared<rsc::RenderScene>())
{
}

World::~World()
{
	delete &prefabs;
	delete &meshes;
	delete &materials;
}

void World::BeginFrame(engn::FrameContext& frame)
{
	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this, &frame]()
			   {
				   UpdateRenderScene(frame);
			   },
			   js::Prerequisites(frame.GetStageBeginEvent(engn::EFrameStage::TransferDataForRendering)),
			   js::JobDef().ExecuteBefore(frame.GetStageBeginEvent(engn::EFrameStage::RenderingBegin)));
}

PrefabInstanceHandle World::SpawnPrefab(const as::PrefabAssetHandle& prefab, const PrefabSpawnParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (!prefab.IsValid())
	{
		SPT_LOG_ERROR(World, "Trying to spawn invalid prefab!");
		return PrefabInstanceHandle{};
	}

	Transform prefabTransform;
	prefabTransform.location = params.location;
	prefabTransform.rotation = params.rotation;
	prefabTransform.scale    = params.scale;

	PrefabInstanceHandle instanceHandle = prefabs.instances.Add();
	SPT_CHECK(instanceHandle.IsValid());

	PrefabInstance& instance = prefabs.instances.GetRef(instanceHandle);
	instance.prefab    = prefab;
	instance.transform = prefabTransform;

	SpawnContext spawnContext
	{
		.assetsSystem   = prefab->GetOwningSystem(),
		.world          = *this,
		.prefabInstance = instance,
		.transform      = prefabTransform
	};

	prefab->Spawn(&spawnContext);

	return instanceHandle;
}

void World::DestroyPrefabInstance(PrefabInstanceHandle instanceHandle)
{
	prefabs.instances.Delete(instanceHandle);
}

void World::SetBiome(const rsc::BiomeDefinition& biome, lib::DynamicArray<as::PrefabAssetHandle> placementAssets)
{
	m_placementSystem.SetBiome(biome, std::move(placementAssets));
}

void World::ProcessPlacements(rsc::SceneRendererHandle sceneRenderer)
{
	m_placementSystem.ProcessPlacements(*this, sceneRenderer);
}

rsc::PlacementCommand World::CreatePlacementCommand(engn::FrameContext& frame, math::Vector3f location)
{
	return m_placementSystem.CreatePlacementCommand(frame, location);
}

void World::UpdateRenderScene(engn::FrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	rsc::RenderScene& renderScene = GetRenderSceneRef();

	lib::MemoryArena perInstanceArena = frame.GetFrameMemoryArena().CreateSubArena("Update Render Instance", 1024u * 4u);

	const auto updateRenderMesh = [this, &perInstanceArena, &renderScene](MeshesChunkHandle chunkHandle, MeshesChunk& chunk)
	{
		for (MeshEntity& mesh : chunk.meshes)
		{
			SPT_PROFILER_SCOPE("Update Render Mesh");

			if (!mesh.render.instance.IsValid())
			{
				rsc::RenderInstanceDef instanceDef;
				instanceDef.transform = mesh.def.owningPrefab->transform.GetAffineTransform() * mesh.def.transform.GetAffineTransform();

				mesh.render.instance = renderScene.CreateInstance(instanceDef);
			}

			if (!mesh.render.renderMaterialSlots.IsValid())
			{
				const Uint32 materialsNum = static_cast<Uint32>(mesh.def.mesh->GetRenderMesh()->GetSubmeshes().size());
				const lib::Span<ecs::EntityHandle> slotsArray = perInstanceArena.AllocateSpanUninitialized<ecs::EntityHandle>(materialsNum);

				Uint32 slotIdx = 0u;
				MaterialAssetSlotsChunkHandle currentChunk = mesh.def.materialSlots;
				while (currentChunk.IsValid())
				{
					const MaterialAssetSlotsChunk& matsChunk = materials.slots.GetRef(currentChunk);
					for (Uint32 idx = 0u; idx < matsChunk.slots.size(); ++idx)
					{
						slotsArray[slotIdx++] = matsChunk.slots[idx].material->GetMaterialEntity();
					}

					currentChunk = matsChunk.next;
				}
				
				mesh.render.renderMaterialSlots = renderScene.materials.CreateMaterialSlotsChain(slotsArray);
			}

			if (!mesh.render.draw.IsValid())
			{
				mesh.render.draw = renderScene.draws.draws.Add(rsc::RetainedDraw
					{
						.instance      = mesh.render.instance,
						.mesh          = *mesh.def.mesh->GetRenderMesh(),
						.materialSlots = mesh.render.renderMaterialSlots
					});
			}

			if (!mesh.render.rtInstance.IsValid())
			{
				mesh.render.rtInstance = renderScene.rt.instances.Add(
					rsc::RTInstance
					{
						.instance      = mesh.render.instance,
						.rtGeometry    = *mesh.def.mesh->GetRenderMesh(),
						.materialSlots = mesh.render.renderMaterialSlots
					});
			}

			perInstanceArena.Reset();
		}
	};

	const auto preDeleteInstance = [this, &renderScene](PrefabInstanceHandle instanceHandle, PrefabInstance& instance)
	{
		MeshesChunkHandle currentChunk = instance.meshesChunk;

		while (currentChunk.IsValid())
		{
			MeshesChunk& chunk = meshes.chunks.GetRef(currentChunk);
			for (MeshEntity& mesh : chunk.meshes)
			{
				SPT_PROFILER_SCOPE("Delete Render Mesh");

				if (mesh.def.materialSlots.IsValid())
				{
					materials.DestroyMaterialSlotsChain(mesh.def.materialSlots);
				}

				if (mesh.render.draw.IsValid())
				{
					renderScene.draws.draws.Delete(mesh.render.draw);
				}

				if (mesh.render.rtInstance.IsValid())
				{
					renderScene.rt.instances.Delete(mesh.render.rtInstance);
				}

				if (mesh.render.instance.IsValid())
				{
					renderScene.DeleteInstance(mesh.render.instance);
				}

				if (mesh.render.renderMaterialSlots.IsValid())
				{
					renderScene.materials.DestroyMaterialSlotsChain(mesh.render.renderMaterialSlots);
				}
			}

			MeshesChunkHandle prevChunk = currentChunk;
			currentChunk = chunk.next;

			meshes.chunks.Delete(prevChunk);
		}
	};

	prefabs.instances.Flush(preDeleteInstance);

	meshes.chunks.ForEachDirty(updateRenderMesh);

	meshes.chunks.Flush();
	materials.slots.Flush();

	renderScene.PostFrameDataUpdate(frame);
}

} // spt::gf
