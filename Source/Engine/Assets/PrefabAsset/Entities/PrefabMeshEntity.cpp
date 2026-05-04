#include "PrefabMeshEntity.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "PrefabAsset.h"
#include "MeshAsset.h"
#include "MaterialAsset.h"
#include "AssetsSystem.h"
#include "RenderScene.h"
#include "StaticMeshes/RenderMesh.h"


SPT_DEFINE_LOG_CATEGORY(PrefabMeshEntity, true);

namespace spt::as
{

struct CompiledMeshEntity
{
	math::Affine3f transform = math::Affine3f::Identity();

	ResourcePathID mesh = InvalidResourcePathID;
	Uint32 materialsNum = 0u;
};


lib::DynamicArray<Byte> PrefabMeshEntity::Compile(PrefabCompiler& compiler) const
{
	const Uint32 materialsNum = static_cast<Uint32>(materials.size());
	const Uint32 materialsDataSize = materialsNum * sizeof(ResourcePathID);

	CompiledMeshEntity compiledEntity;
	compiledEntity.transform = math::Affine3f::Identity();
	compiledEntity.transform.prescale(scale);
	compiledEntity.transform.prerotate(math::Utils::EulerToRotationMatrixDegrees(rotation.x(), rotation.y(), rotation.z()));
	compiledEntity.transform.pretranslate(location);

	compiledEntity.mesh = prefab_compiler_api::CreateAssetDependency(compiler, mesh);

	compiledEntity.materialsNum = materialsNum;

	const Uint32 compiledDataSize = sizeof(CompiledMeshEntity) + materialsDataSize;
	lib::DynamicArray<Byte> compiledData(compiledDataSize);
	std::memcpy(compiledData.data(), &compiledEntity, sizeof(CompiledMeshEntity));

	lib::Span<ResourcePathID> outMaterialIDs(reinterpret_cast<ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), materialsNum);
	for(Uint32 matIdx = 0u; matIdx < materialsNum; ++matIdx)
	{
		outMaterialIDs[matIdx] = prefab_compiler_api::CreateAssetDependency(compiler, materials[matIdx]);
	}

	return compiledData;
}

void PrefabMeshEntity::Spawn(const PrefabSpawningContext& context, lib::Span<const Byte> compiledData)
{
	const CompiledMeshEntity& compiledMesh = *reinterpret_cast<const CompiledMeshEntity*>(compiledData.data());

	MeshAssetHandle meshAsset = context.assetsSystem.GetLoadedAsset<MeshAsset>(compiledMesh.mesh);
	if (!meshAsset.IsValid())
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - mesh asset ({}) is invalid. Resolved path: {}", compiledMesh.mesh, context.assetsSystem.ResolvePath(compiledMesh.mesh).GetPath().string());
		return;
	}

	if (meshAsset->GetSubmeshesNum() != compiledMesh.materialsNum)
	{
		SPT_LOG_ERROR(PrefabMeshEntity, "Failed to spawn PrefabMeshEntity - number of materials ({}) does not match number of submeshes ({}) in mesh asset ({})", compiledMesh.materialsNum, meshAsset->GetSubmeshesNum(), compiledMesh.mesh);
		return;
	}

	const lib::Span<const ResourcePathID> materialAssets(reinterpret_cast<const ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), compiledMesh.materialsNum);

	const math::Affine3f localTransform = compiledMesh.transform;

	const math::Affine3f transform = context.transform * localTransform;

	rsc::RenderInstanceDef instanceDef;
	instanceDef.transform = transform;
	const rsc::RenderInstanceHandle instance = context.scene.CreateInstance(instanceDef);

	lib::InlineDynamicArray<ecs::EntityHandle, 256u> materialSlotsArray;
	for (Uint32 matIdx = 0u; matIdx < materialAssets.size(); ++matIdx)
	{
		const MaterialAssetHandle material = context.assetsSystem.GetLoadedAsset<MaterialAsset>(materialAssets[matIdx]);
		materialSlotsArray.EmplaceBack(material->GetMaterialEntity());
	}

	const rsc::MaterialSlotsChunkHandle materialSlotsHandle = context.scene.materials.CreateMaterialSlotsChain({ materialSlotsArray.GetData(), materialSlotsArray.GetSize()});

	context.scene.draws.draws.Add(
			rsc::RetainedDraw
			{
				.instance      = instance,
				.mesh          = *meshAsset->GetRenderMesh(),
				.materialSlots = materialSlotsHandle
			});

	context.scene.rt.instances.Add(
			rsc::RTInstance
			{
				.instance      = instance,
				.rtGeometry    = *meshAsset->GetRenderMesh(),
				.materialSlots = materialSlotsHandle
			});
}

} // spt::as
