#include "GLTFImporter.h"
#include "AssetsSystem.h"
#include "Eigen/src/Geometry/Transform.h"
#include "Eigen/src/Geometry/Translation.h"
#include "Loaders/GLTF.h"
#include "MaterialInstance/PBRMaterialInstance.h"
#include "MeshAsset.h"
#include "PrefabAsset.h"
#include "ProfilerCore.h"

SPT_DEFINE_LOG_CATEGORY(GLTFImporter, true);


namespace spt::as::importer
{

static math::Affine3f GetNodeTransform(const tinygltf::Node& node)
{
	if (!node.matrix.empty())
	{
		SPT_CHECK(node.matrix.size() == 16);
		const math::Matrix4f matrix(math::Map<const math::Matrix4d>(node.matrix.data()).cast<float>());
		return math::Affine3f(matrix);
	}

	math::Affine3f transform = math::Affine3f::Identity();

	if (!node.scale.empty())
	{
		SPT_CHECK(node.scale.size() == 3);
		const math::Map<const math::Vector3d> nodeScaleDouble(node.scale.data());
		const math::Vector3f nodeScale = nodeScaleDouble.cast<float>();
		transform.prescale(nodeScale);
	}

	if (!node.rotation.empty())
	{
		SPT_CHECK(node.rotation.size() == 4);
		const math::Vector4f nodeQuaternion = math::Map<const math::Vector4d>(node.rotation.data()).cast<float>();
		const math::Quaternionf quaternion(nodeQuaternion.w(), nodeQuaternion.x(), nodeQuaternion.y(), nodeQuaternion.z());
		transform.prerotate(quaternion);
	}

	if (!node.translation.empty())
	{
		SPT_CHECK(node.translation.size() == 3);
		const math::Vector3f nodeTranslation = math::Map<const math::Vector3d>(node.translation.data()).cast<float>();
		transform.pretranslate(nodeTranslation);
	}

	return transform;
}


static lib::Path GetMaterialAssetRelativePath(const tinygltf::Material& material, int materialIdx)
{
	if (material.name.empty())
	{
		return lib::String("Material_") + std::to_string(materialIdx) + ".sptasset";
	}
	else
	{
		return lib::String("Material_") + material.name + ".sptasset";
	}
}


static lib::Path GetMeshAssetRelativePath(const tinygltf::Mesh& mesh, int meshIdx)
{
	if (mesh.name.empty())
	{
		return lib::String("Mesh_") + std::to_string(meshIdx) + ".sptasset";
	}
	else
	{
		return lib::String("Mesh_") + mesh.name + ".sptasset";
	}
}


void InitPrefabDefinition(PrefabDefinition& prefabDef, const rsc::GLTFModel& model)
{
	struct NodeSpawnInfo
	{
		int idx = -1;
		math::Affine3f transform = math::Affine3f::Identity();
	};

	lib::DynamicArray<NodeSpawnInfo> nodesToSpawn;
	for (const tinygltf::Scene& scene : model.scenes)
	{
		for (const int nodeIdx : scene.nodes)
		{
			nodesToSpawn.emplace_back(NodeSpawnInfo{ .idx = nodeIdx, });
		}
	}

	while (!nodesToSpawn.empty())
	{
		const NodeSpawnInfo node = nodesToSpawn[nodesToSpawn.size() - 1u];
		nodesToSpawn.pop_back();

		const tinygltf::Node& nodeData = model.nodes[node.idx];

		const math::Affine3f localTransform = GetNodeTransform(nodeData);
		const math::Affine3f transform = node.transform * localTransform;

		if (nodeData.mesh != -1)
		{
			const tinygltf::Mesh& mesh = model.meshes[nodeData.mesh];

			const Real32 sx = transform.matrix().block<3, 1>(0, 0).norm();
			const Real32 sy = transform.matrix().block<3, 1>(0, 1).norm();
			const Real32 sz = transform.matrix().block<3, 1>(0, 2).norm();

			math::Matrix3f rotationMat = transform.rotation();
			rotationMat.col(0).normalize();
			rotationMat.col(1).normalize();
			rotationMat.col(2).normalize();

			const math::Vector3f eulerAngles = rotationMat.eulerAngles(0, 1, 2);

			lib::UniquePtr<PrefabMeshEntity> meshEntity = lib::MakeUnique<PrefabMeshEntity>();
			meshEntity->location = transform.translation();
			meshEntity->rotation = math::Vector3f(math::Utils::RadiansToDegrees(eulerAngles.x()), math::Utils::RadiansToDegrees(eulerAngles.y()), math::Utils::RadiansToDegrees(eulerAngles.z()));
			meshEntity->scale    = math::Vector3f(sx, sy, sz);

			meshEntity->mesh = GetMeshAssetRelativePath(mesh, nodeData.mesh);

			for (const tinygltf::Primitive& prim : mesh.primitives)
			{
				meshEntity->materials.emplace_back(GetMaterialAssetRelativePath(model.materials[prim.material], prim.material));
			}

			PrefabEntityDefinition entityDef;
			entityDef.entity = std::move(meshEntity);
			prefabDef.entities.emplace_back(std::move(entityDef));
		}

		for (int childIdx : nodeData.children)
		{
			nodesToSpawn.emplace_back(NodeSpawnInfo{ .idx = childIdx, .transform = transform });
		}
	}
}
void InitPrefabDefinition(PrefabDefinition& prefabDef, const AssetInstance& asset, const GLTFPrefabDefinition& gltfDef)
{
	const lib::Path referencePath  = asset.GetDirectoryPath();
	const lib::Path gltfSourcePath = !gltfDef.gltfSourcePath.empty() ? (referencePath / gltfDef.gltfSourcePath) : lib::Path{};
	const lib::String gltfSourcePathAsString = gltfSourcePath.generic_string();

	const std::optional<rsc::GLTFModel>& model = asset.GetOwningSystem().GetCompilationInputData<std::optional<rsc::GLTFModel>>(gltfSourcePathAsString,
			[&gltfSourcePathAsString]()
			{
				return rsc::LoadGLTFModel(gltfSourcePathAsString);
			});

	if (!model)
	{
		return;
	}

	InitPrefabDefinition(prefabDef, *model);
}

void ImportGLTF(const GLTFImportParams& params)
{
	SPT_PROFILER_FUNCTION();

	const std::optional<rsc::GLTFModel> model = rsc::LoadGLTFModel(params.srcGLTFPath.generic_string());
	if (!model)
	{
		SPT_LOG_ERROR(GLTFImporter, "Failed to load GLTF model from path: {}", params.srcGLTFPath.generic_string());
		return;
	}

	const lib::Path relativeGLTFPath = std::filesystem::relative(params.srcGLTFPath, params.assetsSystem.GetContentPath() / params.dstContentPath);

	for (SizeType meshIdx = 0u; meshIdx < model->meshes.size(); ++meshIdx)
	{
		const tinygltf::Mesh& mesh = model->meshes[meshIdx];

		const lib::Path meshPath = params.dstContentPath / GetMeshAssetRelativePath(mesh, static_cast<int>(meshIdx));

		const MeshSourceDefinition meshSource
		{
			.path     = relativeGLTFPath,
			.meshIdx  = static_cast<Uint32>(meshIdx),
			.meshName = mesh.name
		};
		MeshDataInitializer meshInitializer(meshSource);

		params.assetsSystem.CreateAsset(AssetInitializer
			{
				.type            = CreateAssetType<MeshAsset>(),
				.path            = meshPath,
				.dataInitializer = &meshInitializer
			});

		SPT_LOG_INFO(GLTFImporter, "Imported mesh asset: {} from GLTF: {}", meshPath.generic_string(), params.srcGLTFPath.generic_string());
	}

	for (SizeType materialIdx = 0u; materialIdx < model->materials.size(); ++materialIdx)
	{
		const tinygltf::Material& material = model->materials[materialIdx];

		const lib::Path materialPath = params.dstContentPath / GetMaterialAssetRelativePath(material, static_cast<int>(materialIdx));

		const PBRGLTFMaterialDefinition materialDefinition
		{
			.gltfSourcePath = relativeGLTFPath,
			.materialName   = material.name,
			.materialIdx    = static_cast<Uint32>(materialIdx)
		};
		PBRGLTFMaterialInitializer materialInitializer(materialDefinition);

		params.assetsSystem.CreateAsset(AssetInitializer
			{
				.type            = CreateAssetType<MaterialAsset>(),
				.path            = materialPath,
				.dataInitializer = &materialInitializer
			});

		SPT_LOG_INFO(GLTFImporter, "Imported material asset: {} from GLTF: {}", materialPath.generic_string(), params.srcGLTFPath.generic_string());
	}

	const lib::Path prefabName = "Prefab.sptasset";

	if (params.importPrefabAsGLTF)
	{
		GLTFPrefabDataInitializer prefabInitializer(GLTFPrefabDefinition
			{
				.gltfSourcePath = relativeGLTFPath
			});

		params.assetsSystem.CreateAsset(AssetInitializer
			{
				.type = CreateAssetType<PrefabAsset>(),
				.path = params.dstContentPath / prefabName,
				.dataInitializer = &prefabInitializer
			});
	}
	else
	{
		PrefabDefinition prefabDef;
		InitPrefabDefinition(prefabDef, *model);

		PrefabDataInitializer standardPrefabInitializer(std::move(prefabDef));

		params.assetsSystem.CreateAsset(AssetInitializer
			{
				.type            = CreateAssetType<PrefabAsset>(),
				.path            = params.dstContentPath / prefabName,
				.dataInitializer = &standardPrefabInitializer
			});
	}

	SPT_LOG_INFO(GLTFImporter, "Imported prefab asset: {} from GLTF: {}", (params.dstContentPath / prefabName).generic_string(), params.srcGLTFPath.generic_string());
}

} // spt::as::importer
