#include "glTFSceneLoader.h"
#include "ResourcesManager.h"
#include "Paths.h"
#include "GLTFMeshBuilder.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
#include "Types/Texture.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Renderer.h"
#include "Transfers/UploadUtils.h"
#include "Transfers/TransfersManager.h"
#include "Utils/TextureUtils.h"
#include "Types/AccelerationStructure.h"
#include "RayTracing/RayTracingSceneTypes.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "DeviceQueues/GPUWorkload.h"


namespace spt::rsc
{

SPT_DEFINE_LOG_CATEGORY(LogGLTFLoader, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// glTFLoader ====================================================================================

namespace glTFLoader
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

		// Ensure uniform scale
		//SPT_CHECK(math::Utils::AreNearlyEqual(node.scale[0], node.scale[1]));
		//SPT_CHECK(math::Utils::AreNearlyEqual(node.scale[0], node.scale[2]));

		const math::Map<const math::Vector3d> nodeScaleDouble(node.scale.data());
		const math::Vector3f nodeScale = nodeScaleDouble.cast<float>();

		const math::AlignedScaling3f scale(nodeScale.x(), nodeScale.y(), nodeScale.z());
		transform = scale * transform;
	}

	if (!node.rotation.empty())
	{
		SPT_CHECK(node.rotation.size() == 4);
		const math::Vector4f nodeQuaternion = math::Map<const math::Vector4d>(node.rotation.data()).cast<float>();

		const math::Quaternionf quaternion(nodeQuaternion.x(), nodeQuaternion.y(), nodeQuaternion.z(), nodeQuaternion.w());
		transform = quaternion.matrix() * transform;
	}

	if (!node.translation.empty())
	{
		SPT_CHECK(node.translation.size() == 3);
		const math::Vector3f nodeTranslation = math::Map<const math::Vector3d>(node.translation.data()).cast<float>();

		const math::Translation3f translation(nodeTranslation.x(), nodeTranslation.y(), nodeTranslation.z());
		transform = translation * transform;
	}

	return transform;
}

static lib::MTHandle<RenderMesh> LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model, const MeshBuildParameters& parameters)
{
	SPT_PROFILER_FUNCTION();

	GLTFMeshBuilder builder(parameters);
	builder.BuildMesh(mesh, model);

	return builder.EmitMeshGeometry();
}

static rhi::EFragmentFormat GetImageFormat(const tinygltf::Image& image)
{
	if (image.component == 1)
	{
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::R8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::R16_UN_Float; 
		}
	}
	else if (image.component == 2)
	{
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::RG8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::RG16_UN_Float; 
		}
	}
	else if (image.component == 3)
	{
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::RGB8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::RGB16_UN_Float; 
		}
	}
	else if (image.component == 4)
	{
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::RGBA8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::RGBA16_UN_Float; 
		}
	}

	SPT_CHECK_NO_ENTRY();
	return rhi::EFragmentFormat::None;
}

static lib::SharedRef<rdr::Texture> CreateImage(const tinygltf::Image& image)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIAllocationInfo allocationInfo(rhi::EMemoryUsage::GPUOnly);

	const Uint32 mipLevels = math::Utils::ComputeMipLevelsNumForResolution(math::Vector2u(image.width, image.height));

	rhi::TextureDefinition textureDef;
	textureDef.resolution		= math::Vector3u(static_cast<Uint32>(image.width), static_cast<Uint32>(image.height), 1);
	textureDef.usage			= lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::TransferSource);
	textureDef.format			= GetImageFormat(image);
	textureDef.samples			= 1;
	textureDef.mipLevels		= mipLevels;
	textureDef.arrayLayers		= 1;

	return rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(image.name), textureDef, allocationInfo);
}

static lib::DynamicArray<lib::SharedPtr<rdr::TextureView>> LoadImages(const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("LoadedImageBarriersContext"), rhi::ContextDefinition());

	const SizeType imagesNum = model.images.size();

	lib::DynamicArray<lib::SharedRef<rdr::Texture>> textures;
	textures.reserve(imagesNum);

	// Create all textures
	for (const tinygltf::Image& image : model.images)
	{
		textures.emplace_back(CreateImage(image));
	}

	{
		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("PreTransferDependencyCmdBuffer"),
																											context,
																											cmdBufferDef);

		rhi::RHIDependency preTransferDependency;
		for (const auto& texture : textures)
		{
			const SizeType dependencyIdx = preTransferDependency.AddTextureDependency(texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			preTransferDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::TransferDest);
		}
		commandRecorder->ExecuteBarrier(preTransferDependency);

		const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = commandRecorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload);

		gpuWorkload->Wait();
	}

	// Upload data to textures
	for (Uint32 imageIdx = 0; imageIdx < imagesNum; ++imageIdx)
	{
		const tinygltf::Image& image = model.images[imageIdx];

		const lib::SharedRef<rdr::Texture>& texture = textures[imageIdx];
		const math::Vector3u& resolution = texture->GetResolution();

		gfx::UploadDataToTexture(reinterpret_cast<const Byte*>(image.image.data()), image.image.size(), textures[imageIdx], rhi::ETextureAspect::Color, resolution);
	}

	gfx::FlushPendingUploads();

	// Transition barrier to shader read only
	{
		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Default);
		lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("PostTransferDependencyCmdBuffer"),
																											context,
																											cmdBufferDef);

		for (const auto& texture : textures)
		{
			rdr::utils::GenerateMipMaps(*commandRecorder, texture, rhi::ETextureAspect::Color, 0, rhi::TextureTransition::ShaderRead);
		}

		const lib::SharedRef<rdr::GPUWorkload> workload = commandRecorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(workload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::MemoryTransfersWait, rdr::EGPUWorkloadSubmitFlags::CorePipe));
	}

	lib::DynamicArray<lib::SharedPtr<rdr::TextureView>> textureViews;
	textureViews.reserve(textures.size());

	for (const auto& texture : textures)
	{
		lib::SharedPtr<rdr::TextureView>& textureView = textureViews.emplace_back();
		textureView = texture->CreateView(RENDERER_RESOURCE_NAME(texture->GetRHI().GetName().ToString() + "View"));
		mat::MaterialsUnifiedData::Get().AddMaterialTexture(lib::Ref(textureView));
	}

	return textureViews;
}

static lib::DynamicArray<ecs::EntityHandle> CreateMaterials(const tinygltf::Model& model, const lib::DynamicArray<lib::SharedPtr<rdr::TextureView>>& loadedTextures)
{
	const auto getLoadedTexture = [&model, &loadedTextures](int modelTextureIdx)
	{
		lib::SharedPtr<rdr::TextureView> loadedTexture;
		if (modelTextureIdx != -1)
		{
			const SizeType imageIdx = model.textures[modelTextureIdx].source;
			loadedTexture = loadedTextures[imageIdx];
		}
		return loadedTexture;
	};

	lib::DynamicArray<ecs::EntityHandle> materials;
	materials.reserve(model.materials.size());

	for (const tinygltf::Material& materialSourceDef : model.materials)
	{
		const tinygltf::PbrMetallicRoughness& pbrDef = materialSourceDef.pbrMetallicRoughness;

		float emissiveStrength = 0.f;
		const auto emissiveStrengthIter = materialSourceDef.extensions.find("KHR_materials_emissive_strength");
		if (emissiveStrengthIter != materialSourceDef.extensions.end())
		{
			emissiveStrength = static_cast<Real32>(emissiveStrengthIter->second.Get("emissiveStrength").GetNumberAsDouble());
		}

		rsc::MaterialPBRData pbrData;
		pbrData.baseColorFactor             = math::Map<const math::Vector3d>(pbrDef.baseColorFactor.data()).cast<Real32>();
		pbrData.metallicFactor              = static_cast<Real32>(pbrDef.metallicFactor);
		pbrData.roughnessFactor             = static_cast<Real32>(pbrDef.roughnessFactor);
		pbrData.baseColorTexture            = getLoadedTexture(pbrDef.baseColorTexture.index);
		pbrData.metallicRoughnessTexture    = getLoadedTexture(pbrDef.metallicRoughnessTexture.index);
		pbrData.normalsTexture              = getLoadedTexture(materialSourceDef.normalTexture.index);
		pbrData.emissiveFactor              = math::Map<const math::Vector3d>(materialSourceDef.emissiveFactor.data()).cast<Real32>() * emissiveStrength;
		pbrData.emissiveTexture             =getLoadedTexture(materialSourceDef.emissiveTexture.index);

		const Bool isTransparent = materialSourceDef.alphaMode == "BLEND";
		const Bool isMasked      = materialSourceDef.alphaMode == "MASK";

		mat::MaterialDefinition materialDefinition;
		materialDefinition.name          = materialSourceDef.name;
		materialDefinition.customOpacity = isMasked;
		materialDefinition.transparent   = isTransparent;
		//materialDefinition.doubleSided   = materialSourceDef.doubleSided;
		materialDefinition.doubleSided   = true;
		materialDefinition.emissive      = materialSourceDef.emissiveFactor[0] > 0.f || materialSourceDef.emissiveFactor[1] > 0.f || materialSourceDef.emissiveFactor[2] > 0.f;

		const ecs::EntityHandle material = mat::MaterialsSubsystem::Get().CreateMaterial(materialDefinition, pbrData);

		materials.emplace_back(material);
	}

	return materials;
}

void BuildAccelerationStructures(const rdr::BLASBuilder& builder)
{
	SPT_PROFILER_FUNCTION();

	if (!builder.IsEmpty())
	{
		lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("BuildBLASesContext"), rhi::ContextDefinition());

		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Default);;
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("BuildBLASesCmdBuffer"),
																									 context,
																									 cmdBufferDef);

		builder.Build(*recorder);

		const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(workload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::MemoryTransfersWait, rdr::EGPUWorkloadSubmitFlags::CorePipe));
	}
}

void LoadScene(RenderScene& scene, lib::StringView path)
{
	SPT_PROFILER_FUNCTION();

	const lib::StringView fileExtension = engn::Paths::GetExtension(path);
	SPT_CHECK(fileExtension == "gltf" || fileExtension == "glb");

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	loader.SetStoreOriginalJSONForExtrasAndExtensions(false);

	lib::String error;
	lib::String warning;

	const Bool isBinary = fileExtension == "glb";

	lib::String pathAsString(path);

	const Bool loaded = isBinary ? loader.LoadBinaryFromFile(&model, &error, &warning, pathAsString)
								 : loader.LoadASCIIFromFile(&model, &error, &warning, pathAsString);

	if (!warning.empty())
	{
		SPT_LOG_WARN(LogGLTFLoader, warning.data());
	}

	if (!error.empty())
	{
		SPT_LOG_ERROR(LogGLTFLoader, error.data());
	}

	if (loaded)
	{
		const Bool withRayTracing = rdr::Renderer::IsRayTracingEnabled();

		const lib::DynamicArray<lib::SharedPtr<rdr::TextureView>> loadedTextures = LoadImages(model);
		const lib::DynamicArray<ecs::EntityHandle> materials = CreateMaterials(model, loadedTextures);

		lib::HashMap<int, lib::MTHandle<RenderMesh>> loadedMeshes;
		loadedMeshes.reserve(model.meshes.size());

		rdr::BLASBuilder blasBuilder;

		MeshBuildParameters meshBuildParams;
		if (withRayTracing)
		{
			meshBuildParams.blasBuilder = &blasBuilder;
		}

		for(size_t meshIdx = 0u; meshIdx < model.meshes.size(); ++meshIdx)
		{
			const tinygltf::Mesh& mesh = model.meshes[meshIdx];
			if (std::any_of(mesh.primitives.cbegin(), mesh.primitives.cend(), [](const tinygltf::Primitive& prim) { return prim.material != -1; }))
			{
				loadedMeshes.emplace(static_cast<int>(meshIdx), LoadMesh(mesh, model, meshBuildParams));
			}
		}

		Uint32 createdEntitiesNum = 0;

		struct NodeSpawnData
		{
			math::Affine3f parentTransform;
			int nodeIdx = -1;
		};

		lib::DynamicArray<NodeSpawnData> nodesToSpawn;

		math::Affine3f sceneTransform = math::Affine3f::Identity();

		//sceneTransform = math::AlignedScaling3f(50.f, 50.f, 50.f) * sceneTransform;
		//sceneTransform = math::AlignedScaling3f(0.01f, 0.01f, 0.01f) * sceneTransform;
		sceneTransform = math::Utils::EulerToQuaternionDegrees(90.f, 0.f, 0.f) * sceneTransform;
		//sceneTransform = math::Utils::EulerToQuaternionDegrees(180.f, 0.f, 0.f) * sceneTransform;
		//sceneTransform = math::Utils::EulerToQuaternionDegrees(-90.f, 0.f, 0.f) * sceneTransform;

		for (const tinygltf::Scene& gltfScene : model.scenes)
		{
			for (const int nodeIdx : gltfScene.nodes)
			{
				nodesToSpawn.emplace_back(NodeSpawnData{ sceneTransform, nodeIdx });
			}
		}

		for (SizeType spawnIdx = 0u; spawnIdx < nodesToSpawn.size(); ++spawnIdx)
		{
			const NodeSpawnData& spawnData = nodesToSpawn[spawnIdx];

			const tinygltf::Node& node = model.nodes[spawnData.nodeIdx];

			const math::Affine3f localTransform = GetNodeTransform(node);

			const math::Affine3f nodeTransform = spawnData.parentTransform * localTransform;

			for (const int childNodeIdx : node.children)
			{
				nodesToSpawn.emplace_back(NodeSpawnData{ nodeTransform, childNodeIdx });
			}

			if (node.mesh != -1 && loadedMeshes.contains(node.mesh))
			{
				const tinygltf::Mesh& mesh = model.meshes[node.mesh];

				RenderInstanceData instanceData;
				instanceData.transformComp.SetTransform(nodeTransform);
				const RenderSceneEntityHandle meshSceneEntity = scene.CreateEntity(instanceData);

				++createdEntitiesNum;

				StaticMeshInstanceRenderData entityStaticMeshData;
				entityStaticMeshData.staticMesh = loadedMeshes[node.mesh];

				rsc::MaterialSlotsComponent materialSlots;

				materialSlots.slots.reserve(mesh.primitives.size());
				for (const tinygltf::Primitive& prim : mesh.primitives)
				{
					if (prim.material == -1)
					{
						continue;
					}
					materialSlots.slots.emplace_back(materials.at(prim.material));
				}

				meshSceneEntity.emplace<StaticMeshInstanceRenderData>(entityStaticMeshData);
				meshSceneEntity.emplace<rsc::MaterialSlotsComponent>(std::move(materialSlots));

				if (withRayTracing)
				{
					RayTracingGeometryProviderComponent rtGeoProvider;
					rtGeoProvider.provider = loadedMeshes[node.mesh].Get();
					meshSceneEntity.emplace<RayTracingGeometryProviderComponent>(rtGeoProvider);
				}
			}
		}


		if (withRayTracing)
		{
			gfx::FlushPendingUploads();

			BuildAccelerationStructures(blasBuilder);
		}

		SPT_LOG_TRACE(LogGLTFLoader, "Finished loading {} entities", createdEntitiesNum);
	}
}

} // glTFLoader

} // spt::rsc