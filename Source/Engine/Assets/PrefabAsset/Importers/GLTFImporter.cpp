#include "GLTFImporter.h"
#include "AssetsSystem.h"
#include "Eigen/src/Geometry/Transform.h"
#include "Eigen/src/Geometry/Translation.h"
#include "Loaders/GLTF.h"
#include "MaterialInstance/PBRMaterialInstance.h"
#include "MeshAsset.h"
#include "PrefabAsset.h"
#include "ProfilerCore.h"

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

	if (!node.translation.empty())
	{
		SPT_CHECK(node.translation.size() == 3);
		const math::Vector3f nodeTranslation = math::Map<const math::Vector3d>(node.translation.data()).cast<float>();
		transform.pretranslate(nodeTranslation);
	}

	if (!node.rotation.empty())
	{
		SPT_CHECK(node.rotation.size() == 4);
		const math::Vector4f nodeQuaternion = math::Map<const math::Vector4d>(node.rotation.data()).cast<float>();
		const math::Quaternionf quaternion(nodeQuaternion.x(), nodeQuaternion.y(), nodeQuaternion.z(), nodeQuaternion.w());
		transform.prerotate(quaternion);
	}

	if (!node.scale.empty())
	{
		SPT_CHECK(node.scale.size() == 3);
		const math::Map<const math::Vector3d> nodeScaleDouble(node.scale.data());
		const math::Vector3f nodeScale = nodeScaleDouble.cast<float>();
		transform.prescale(nodeScale);
	}

	return transform;
}


static lib::Path GetMaterialAssetRelativePath(const tinygltf::Material& material, int materialIdx)
{
	return lib::String("Material_") + std::to_string(materialIdx) + ".sptasset";
}


static lib::Path GetMeshAssetRelativePath(const tinygltf::Mesh& mesh, int meshIdx)
{
	return lib::String("Mesh_") + std::to_string(meshIdx) + ".sptasset";
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

	struct NodeSpawnInfo
	{
		int idx = -1;
		math::Affine3f transform = math::Affine3f::Identity();
	};

	lib::DynamicArray<NodeSpawnInfo> nodesToSpawn;
	for (const tinygltf::Scene& scene : model->scenes)
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

		const tinygltf::Node& nodeData = model->nodes[node.idx];

		const math::Affine3f localTransform = GetNodeTransform(nodeData);
		const math::Affine3f transform = node.transform * localTransform;

		if (nodeData.mesh != -1)
		{
			const tinygltf::Mesh& mesh = model->meshes[nodeData.mesh];

			const math::Matrix3f rotationMat = transform.rotation();

			const Real32 sx = transform.matrix().block<3, 1>(0, 0).norm();
			const Real32 sy = transform.matrix().block<3, 1>(0, 1).norm();
			const Real32 sz = transform.matrix().block<3, 1>(0, 2).norm();

			lib::UniquePtr<PrefabMeshEntity> meshEntity = lib::MakeUnique<PrefabMeshEntity>();
			meshEntity->location = transform.translation();
			meshEntity->rotation = rotationMat.eulerAngles(0, 1, 2);
			meshEntity->scale    = math::Vector3f(sx, sy, sz);

			meshEntity->mesh = GetMeshAssetRelativePath(mesh, nodeData.mesh);

			for (const tinygltf::Primitive& prim : mesh.primitives)
			{
				meshEntity->materials.emplace_back(GetMaterialAssetRelativePath(model->materials[prim.material], prim.material));
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

void ImportGLTF(const GLTFImportParams& params)
{
	SPT_PROFILER_FUNCTION();

	const std::optional<rsc::GLTFModel> model = rsc::LoadGLTFModel(params.srcGLTFPath.generic_string());
	if (!model)
	{
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
	}

	GLTFPrefabDataInitializer prefabInitializer(GLTFPrefabDefinition
		{
			.gltfSourcePath = relativeGLTFPath
		});

	const lib::Path prefabName = params.srcGLTFPath.filename().replace_extension("sptasset");
	params.assetsSystem.CreateAsset(AssetInitializer
		{
			.type = CreateAssetType<PrefabAsset>(),
			.path = params.dstContentPath / prefabName,
			.dataInitializer = &prefabInitializer
		});
}

} // spt::as::importer
