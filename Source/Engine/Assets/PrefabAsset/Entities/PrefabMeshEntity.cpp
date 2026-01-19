#include "PrefabMeshEntity.h"
#include "PrefabAsset.h"
#include "MeshAsset.h"
#include "MaterialAsset.h"
#include "AssetsSystem.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
#include "StaticMeshes/RenderMesh.h"


namespace spt::as
{

struct CompiledMeshEntity
{
	math::Vector3f    location = math::Vector3f::Zero();
	math::Quaternionf rotation = math::Quaternionf::Identity();
	math::Vector3f    scale    = math::Vector3f::Ones();

	ResourcePathID mesh = InvalidResourcePathID;
	Uint32 materialsNum = 0u;
};


lib::DynamicArray<Byte> PrefabMeshEntity::Compile(PrefabCompiler& compiler) const
{
	const Uint32 materialsNum = static_cast<Uint32>(materials.size());
	const Uint32 materialsDataSize = materialsNum * sizeof(ResourcePathID);

	CompiledMeshEntity compiledEntity;
	compiledEntity.location = location;
	compiledEntity.rotation = math::Utils::EulerToQuaternionDegrees(rotation.x(), rotation.y(), rotation.z());
	compiledEntity.scale    = scale;

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
		return;
	}

	if (meshAsset->GetSubmeshesNum() != compiledMesh.materialsNum)
	{
		return;
	}

	const lib::Span<const ResourcePathID> materials(reinterpret_cast<const ResourcePathID*>(compiledData.data() + sizeof(CompiledMeshEntity)), compiledMesh.materialsNum);

	math::Affine3f localTransform = math::Affine3f::Identity();
	localTransform.prescale(compiledMesh.scale);
	localTransform.prerotate(compiledMesh.rotation);
	localTransform.pretranslate(compiledMesh.location);

	math::Affine3f transform = context.transform * localTransform;

	rsc::RenderInstanceData instanceData;
	instanceData.transformComp.SetTransform(transform);
	const rsc::RenderSceneEntityHandle entity = context.scene.CreateEntity(instanceData);

	rsc::StaticMeshInstanceRenderData staticMeshData;
	staticMeshData.staticMesh = meshAsset->GetRenderMesh();
	entity.emplace<rsc::StaticMeshInstanceRenderData>(staticMeshData);

	rsc::MaterialSlotsComponent materialSlots;
	materialSlots.slots.reserve(materials.size());
	for (Uint32 matIdx = 0u; matIdx < materials.size(); ++matIdx)
	{
		const MaterialAssetHandle material = context.assetsSystem.GetLoadedAsset<MaterialAsset>(materials[matIdx]);
		materialSlots.slots.emplace_back(material->GetMaterialEntity());
	}
	
	entity.emplace<rsc::MaterialSlotsComponent>(std::move(materialSlots));

	rsc::RayTracingGeometryProviderComponent rtGeoProvider;
	rtGeoProvider.provider = meshAsset->GetRenderMesh().Get();
	entity.emplace<rsc::RayTracingGeometryProviderComponent>(rtGeoProvider);
}

} // spt::as
