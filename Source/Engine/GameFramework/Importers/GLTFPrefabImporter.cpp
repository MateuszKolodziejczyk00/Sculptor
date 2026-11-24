#include "GLTFPrefabImporter.h"
#include "Loaders/GLTF.h"
#include "AssetsSystem.h"
#include "MaterialInstance/PBRMaterialInstance.h"
#include "MeshPrefab.h"

SPT_DEFINE_LOG_CATEGORY(GLTFImporter, true);


namespace spt::gf
{

SPT_REGISTER_PREFAB_IMPORTER(GLTFPrefabImporter, ".gltf");

static math::Affine3f GetNodeTransform(const cgltf_node& node)
{
	if (node.has_matrix)
	{
		const math::Matrix4f matrix(math::Map<const math::Matrix4f>(node.matrix));
		return math::Affine3f(matrix);
	}

	math::Affine3f transform = math::Affine3f::Identity();

	if (node.has_scale)
	{
		const math::Vector3f nodeScale = math::Map<const math::Vector3f>(node.scale);
		transform.prescale(nodeScale);
	}

	if (node.has_rotation)
	{
		const math::Quaternionf quaternion(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
		transform.prerotate(quaternion);
	}

	if (node.has_translation)
	{
		const math::Vector3f nodeTranslation = math::Map<const math::Vector3f>(node.translation);
		transform.pretranslate(nodeTranslation);
	}

	return transform;
}


static lib::Path GetMaterialAssetRelativePath(const cgltf_material& material, SizeType materialIdx)
{
	if (!material.name || material.name[0] == 0)
	{
		return lib::String("Material_") + std::to_string(materialIdx) + ".sptasset";
	}
	else
	{
		return lib::String("Material_") + material.name + ".sptasset";
	}
}


static lib::Path GetMeshAssetRelativePath(const cgltf_mesh& mesh, SizeType meshIdx)
{
	if (!mesh.name || mesh.name[0] == 0)
	{
		return lib::String("Mesh_") + std::to_string(meshIdx) + ".sptasset";
	}
	else
	{
		return lib::String("Mesh_") + mesh.name + ".sptasset";
	}
}


void ImportPrefabDefinitionImpl(const rsc::GLTFModel& model, as::PrefabDefinition& prefabDef)
{
	struct NodeSpawnInfo
	{
		const cgltf_node* node = nullptr;
		math::Affine3f transform = math::Affine3f::Identity();
	};

	lib::DynamicArray<NodeSpawnInfo> nodesToSpawn;
	for (const cgltf_scene& scene : lib::Span<const cgltf_scene>(model->scenes, model->scenes_count))
	{
		for (const cgltf_node* sceneNode : lib::Span<const cgltf_node* const>(scene.nodes, scene.nodes_count))
		{
			nodesToSpawn.emplace_back(NodeSpawnInfo{ .node = sceneNode });
		}
	}

	while (!nodesToSpawn.empty())
	{
		const NodeSpawnInfo spawnInfo = nodesToSpawn[nodesToSpawn.size() - 1u];
		nodesToSpawn.pop_back();

		const cgltf_node& nodeData = *spawnInfo.node;

		const math::Affine3f localTransform = GetNodeTransform(nodeData);
		const math::Affine3f transform = spawnInfo.transform * localTransform;

		if (nodeData.mesh)
		{
			const cgltf_mesh& mesh = *nodeData.mesh;

			const Real32 sx = transform.matrix().block<3, 1>(0, 0).norm();
			const Real32 sy = transform.matrix().block<3, 1>(0, 1).norm();
			const Real32 sz = transform.matrix().block<3, 1>(0, 2).norm();

			math::Matrix3f rotationMat = transform.rotation();
			rotationMat.col(0).normalize();
			rotationMat.col(1).normalize();
			rotationMat.col(2).normalize();

			const math::Vector3f eulerAngles = rotationMat.eulerAngles(0, 1, 2);

			lib::UniquePtr<gf::PrefabMeshEntityDefinition> meshEntity = lib::MakeUnique<gf::PrefabMeshEntityDefinition>();
			meshEntity->name     = lib::String(nodeData.name ? nodeData.name : "Node") + "_" + std::to_string(cgltf_node_index(&*model, &nodeData));
			meshEntity->location = transform.translation();
			meshEntity->rotation = math::Vector3f(math::Utils::RadiansToDegrees(eulerAngles.x()), math::Utils::RadiansToDegrees(eulerAngles.y()), math::Utils::RadiansToDegrees(eulerAngles.z()));
			meshEntity->scale    = math::Vector3f(sx, sy, sz);

			meshEntity->mesh = GetMeshAssetRelativePath(mesh, cgltf_mesh_index(&*model, nodeData.mesh));

			for (const cgltf_primitive& prim : lib::Span<const cgltf_primitive>(mesh.primitives, mesh.primitives_count))
			{
				if (prim.material)
				{
					meshEntity->materials.emplace_back(GetMaterialAssetRelativePath(*prim.material, cgltf_material_index(&*model, prim.material)));
				}
			}

			as::PrefabEntityDefinitionEntry entityDef;
			entityDef.entity = std::move(meshEntity);
			prefabDef.entities.emplace_back(std::move(entityDef));
		}

		for (const cgltf_node* child : lib::Span<const cgltf_node* const>(nodeData.children, nodeData.children_count))
		{
			nodesToSpawn.emplace_back(NodeSpawnInfo{ .node = child, .transform = transform });
		}
	}
}


void GLTFPrefabImporter::ImportPrefabDefinition(as::AssetsSystem& assetsSystem, const lib::Path& sourcePath, as::PrefabDefinition& prefabDef)
{
	const lib::String gltfSourcePathAsString = sourcePath.lexically_normal().generic_string();

	const std::optional<rsc::GLTFModel>& model = assetsSystem.GetCompilationInputData<std::optional<rsc::GLTFModel>>(gltfSourcePathAsString,
			[&gltfSourcePathAsString]()
			{
				return rsc::LoadGLTFModel(gltfSourcePathAsString);
			});

	if (!model)
	{
		return;
	}

	ImportPrefabDefinitionImpl(*model, prefabDef);
}


void ImportGLTF(as::AssetsSystem& assetsSystem, const PrefabImportParams& params)
{
	SPT_PROFILER_FUNCTION();

	const std::optional<rsc::GLTFModel> model = rsc::LoadGLTFModel(params.srcGLTFPath.generic_string());
	if (!model)
	{
		SPT_LOG_ERROR(GLTFImporter, "Failed to load GLTF model from path: {}", params.srcGLTFPath.generic_string());
		return;
	}

	const lib::Path relativeGLTFPath = std::filesystem::relative(params.srcGLTFPath, params.assetsSystem.GetContentPath() / params.dstContentPath);

	for (SizeType meshIdx = 0u; meshIdx < (*model)->meshes_count; ++meshIdx)
	{
		const cgltf_mesh& mesh = (*model)->meshes[meshIdx];

		const lib::Path meshPath = params.dstContentPath / GetMeshAssetRelativePath(mesh, meshIdx);

		const as::MeshSourceDefinition meshSource
		{
			.path     = relativeGLTFPath,
			.meshIdx  = static_cast<Uint32>(meshIdx),
			.meshName = mesh.name ? mesh.name : ""
		};
		as::MeshDataInitializer meshInitializer(meshSource);

		params.assetsSystem.CreateAsset(as::AssetInitializer
			{
				.type            = as::CreateAssetType<as::MeshAsset>(),
				.path            = meshPath,
				.dataInitializer = &meshInitializer
			});

		SPT_LOG_INFO(GLTFImporter, "Imported mesh asset: {} from GLTF: {}", meshPath.generic_string(), params.srcGLTFPath.generic_string());
	}

	for (SizeType materialIdx = 0u; materialIdx < (*model)->materials_count; ++materialIdx)
	{
		const cgltf_material& material = (*model)->materials[materialIdx];

		const lib::Path materialPath = params.dstContentPath / GetMaterialAssetRelativePath(material, materialIdx);

		const as::PBRGLTFMaterialDefinition materialDefinition
		{
			.gltfSourcePath = relativeGLTFPath,
			.materialName   = material.name ? material.name : "",
			.materialIdx    = static_cast<Uint32>(materialIdx)
		};
		as::PBRGLTFMaterialInitializer materialInitializer(materialDefinition);

		params.assetsSystem.CreateAsset(as::AssetInitializer
			{
				.type            = as::CreateAssetType<as::MaterialAsset>(),
				.path            = materialPath,
				.dataInitializer = &materialInitializer
			});

		SPT_LOG_INFO(GLTFImporter, "Imported material asset: {} from GLTF: {}", materialPath.generic_string(), params.srcGLTFPath.generic_string());
	}

	const lib::Path prefabName = "Prefab.sptasset";

	if (params.importPrefabAsReference)
	{
		as::GLTFPrefabDataInitializer prefabInitializer(as::GLTFPrefabDefinition
			{
				.gltfSourcePath = relativeGLTFPath
			});

		params.assetsSystem.CreateAsset(as::AssetInitializer
			{
				.type = as::CreateAssetType<as::PrefabAsset>(),
				.path = params.dstContentPath / prefabName,
				.dataInitializer = &prefabInitializer
			});
	}
	else
	{
		as::PrefabDefinition prefabDef;
		ImportPrefabDefinitionImpl(*model, prefabDef);

		as::PrefabDataInitializer standardPrefabInitializer(std::move(prefabDef));

		params.assetsSystem.CreateAsset(as::AssetInitializer
			{
				.type            = as::CreateAssetType<as::PrefabAsset>(),
				.path            = params.dstContentPath / prefabName,
				.dataInitializer = &standardPrefabInitializer
			});
	}

	SPT_LOG_INFO(GLTFImporter, "Imported prefab asset: {} from GLTF: {}", (params.dstContentPath / prefabName).generic_string(), params.srcGLTFPath.generic_string());
}

} // spt::gf
