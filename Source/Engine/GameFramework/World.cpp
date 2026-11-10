#include "World.h"
#include "JobSystem.h"
#include "MeshPrefab.h"
#include "ChildPrefab.h"
#include "LightPrefab.h"
#include "WorldMaterials.h"
#include "RenderScene.h"


SPT_DEFINE_LOG_CATEGORY(World, true);


namespace spt::gf
{

namespace utils
{

static void DestroyChildPrefabs(World& world, const PrefabInstance& instance)
{
	PrefabInstanceHandle currentChild = instance.child;
	while (currentChild.IsValid())
	{
		const PrefabInstanceHandle nextChild = world.prefabs.instances.GetRef(currentChild).next;
		world.DestroyPrefabInstance(currentChild);
		currentChild = nextChild;
	}
}

} // utils

World::World()
	: prefabs(*(new WorldPrefabs()))
	, meshes(*(new WorldMeshes()))
	, lights(*(new WorldLights()))
	, materials(*(new WorldMaterialSlots()))
	, entities(*(new WorldEntities()))
	, m_renderScene(lib::MakeShared<rsc::RenderScene>())
	, renderSceneSettings(*(new rsc::RenderSceneSettings()))
{
}

World::~World()
{
	delete &prefabs;
	delete &meshes;
	delete &lights;
	delete &materials;
	delete &entities;
	delete &renderSceneSettings;
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

PrefabInstanceHandle World::SpawnPrefab(const as::PrefabAssetHandle& prefab, const SpawnParams& params)
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
	instance.entities = entities.CreateEntitiesChunksList(prefab->GetEntitiesNum());

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
	utils::DestroyChildPrefabs(*this, prefabs.instances.GetRef(instanceHandle));

	prefabs.instances.Delete(instanceHandle);
}

void World::SetPrefabParameter(PrefabInstanceHandle instanceHandle, lib::StringView name, const as::PrefabParameterPayload& payload)
{
	const PrefabInstance& instance = prefabs.instances.GetRef(instanceHandle);

	const SetParameterContext context
	{
		.world          = *this,
		.prefabInstance = instance
	};

	instance.prefab->SetParameter(&context, name, payload);
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

void World::SetTerrain(as::TerrainAssetHandle terrainAsset)
{
	m_terrainAsset = std::move(terrainAsset);
}

void World::SetCloudscape(as::CloudscapeAssetHandle cloudscapeAsset)
{
	m_cloudscapeAsset = std::move(cloudscapeAsset);
}

void World::UpdateRenderScene(engn::FrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	rsc::RenderScene& renderScene = GetRenderSceneRef();

	renderScene.settings = renderSceneSettings;

	lib::MemoryArena perInstanceArena = frame.GetFrameMemoryArena().CreateSubArena("Update Render Instance", 1024u * 4u);

	const auto updateRenderMesh = [this, &perInstanceArena, &renderScene](MeshEntityHandle meshHandle, MeshEntity& mesh)
	{
		SPT_PROFILER_SCOPE("Update Render Mesh");

		SPT_CHECK(mesh.def.mesh.IsValid());
		SPT_CHECK(!!mesh.def.owningPrefab)

		if (!mesh.render.instance.IsValid())
		{
			const math::Affine3f transform =  mesh.def.transform.GetAffineTransform();

			rsc::RenderInstanceDef instanceDef;
			instanceDef.transform = mesh.def.owningPrefab->transform.GetAffineTransform() * transform;

			mesh.render.instance = renderScene.CreateInstance(instanceDef);
		}

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

		if (mesh.render.renderMaterialSlots.IsValid())
		{
			renderScene.materials.WriteMaterialSlotsChain(mesh.render.renderMaterialSlots, slotsArray);
		}
		else
		{
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
	};

	const auto updateRenderLight = [&renderScene](LightEntityHandle lightHandle, LightEntity& light)
	{
		SPT_CHECK(!!light.owningPrefab);

		if (light.render.light.IsValid())
		{
			return;
		}

		const math::Affine3f transform = light.owningPrefab->transform.GetAffineTransform() * light.def.transform.GetAffineTransform();

		if (light.def.type == rsc::ELocalLightType::PointLight)
		{
			rsc::PointLightData pointLight;
			pointLight.color             = light.def.color;
			pointLight.luminousPower     = light.def.luminousPower;
			pointLight.location          = transform.translation();
			pointLight.radius            = light.def.range;
			pointLight.flags             = light.def.flags;
			pointLight.iesProfileTexture = light.def.iesProfile.IsValid() ? light.def.iesProfile->GetTextureView() : nullptr;
			light.render.light =renderScene.lighting.pointLights.Add(pointLight);
		}
		else
		{
			SPT_CHECK(light.def.type == rsc::ELocalLightType::SpotLight);

			rsc::SpotLightData spotLight;
			spotLight.color             = light.def.color;
			spotLight.luminousPower     = light.def.luminousPower;
			spotLight.location          = transform.translation();
			spotLight.direction         = (transform.linear() * math::Vector3f::UnitX()).normalized();
			spotLight.range             = light.def.range;
			spotLight.innerHalfAngle    = light.def.innerHalfAngle;
			spotLight.outerHalfAngle    = light.def.outerHalfAngle;
			spotLight.flags             = light.def.flags;
			spotLight.iesProfileTexture = light.def.iesProfile.IsValid() ? light.def.iesProfile->GetTextureView() : nullptr;
			light.render.light =renderScene.lighting.spotLights.Add(spotLight);
		}

		SPT_CHECK(light.render.light.IsValid());
	};

	const auto preDeleteInstance = [this, &renderScene](PrefabInstanceHandle instanceHandle, PrefabInstance& instance)
	{
		LightEntityHandle currentLight = instance.lights;
		while (currentLight.IsValid())
		{
			const LightEntity& light = lights.entities.GetRef(currentLight);

			if (light.render.light.IsValid())
			{
				if (light.render.light.IsPointLight())
				{
					renderScene.lighting.pointLights.Delete(light.render.light.AsPointLight());
				}
				else
				{
					renderScene.lighting.spotLights.Delete(light.render.light.AsSpotLight());
				}
			}

			const LightEntityHandle nextLight = light.next;

			lights.entities.Delete(currentLight);

			currentLight = nextLight;
		}

		MeshEntityHandle currentMesh = instance.meshes;

		while (currentMesh.IsValid())
		{
			SPT_PROFILER_SCOPE("Delete Render Mesh");

			MeshEntity& mesh = meshes.entities.GetRef(currentMesh);

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

			const MeshEntityHandle nextMesh = mesh.next;

			meshes.entities.Delete(currentMesh);

			currentMesh = nextMesh;
		}

		entities.DestroyEntitiesChunksList(instance.entities);
	};

	// Must happen before flushing instances: their deletion destroys material chains that dirty meshes still read
	meshes.entities.ForEachDirty(updateRenderMesh);
	lights.entities.ForEachDirty(updateRenderLight);

	prefabs.instances.Flush(preDeleteInstance);

	meshes.entities.Flush();
	lights.entities.Flush();
	materials.slots.Flush();
	entities.chunks.Flush();

	renderScene.PostFrameDataUpdate(frame);

	renderScene.SetTerrainDefinition(m_terrainAsset.IsValid() ? m_terrainAsset->GetTerrainDefinition() : rsc::TerrainDefinition{});

	renderScene.SetCloudscapeDefinition(m_cloudscapeAsset.IsValid() ? m_cloudscapeAsset->GetCloudscapeDefinition() : rsc::CloudscapeDefinition{});
}

} // spt::gf
