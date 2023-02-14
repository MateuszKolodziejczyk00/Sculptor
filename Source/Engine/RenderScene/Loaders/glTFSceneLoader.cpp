#include "glTFSceneLoader.h"
#include "ResourcesManager.h"
#include "Paths.h"
#include "MeshBuilder.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"
#include "RenderingDataRegistry.h"
#include "Types/Texture.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Renderer.h"
#include "Transfers/UploadUtils.h"
#include "Transfers/TransfersManager.h"
#include "TextureUtils.h"
#include "Materials/MaterialsUnifiedData.h"
#include "Materials/MaterialTypes.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION

#pragma warning(push)
#pragma warning(disable: 4996)
#include "tiny_gltf.h"
#pragma warning(pop)

namespace spt::rsc
{

SPT_DEFINE_LOG_CATEGORY(LogGLTFLoader, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// GLTFPrimitiveBuilder ==========================================================================

class GLTFMeshBuilder : public MeshBuilder
{
public:

	GLTFMeshBuilder();

	void BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);

private:

	const tinygltf::Accessor* GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name);

	void SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	template<typename TDestType>
	Uint32 AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model, const lib::DynamicArray<SizeType>& remapping = lib::DynamicArray<SizeType>{});
};

GLTFMeshBuilder::GLTFMeshBuilder()
{ }

void GLTFMeshBuilder::BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		BeginNewSubmesh();

		SetIndices(model.accessors[prim.indices], model);

		const static lib::String locationsAccessorName = "POSITION";
		if (const tinygltf::Accessor* locationsAccessor = GetAttributeAccessor(prim, model, locationsAccessorName))
		{
			SetLocations(*locationsAccessor, model);
		}

		const static lib::String normalsAccessorName = "NORMAL";
		if (const tinygltf::Accessor* normalsAccessor = GetAttributeAccessor(prim, model, normalsAccessorName))
		{
			SetNormals(*normalsAccessor, model);
		}

		const static lib::String tangentsAccessorName = "TANGENT";
		if (const tinygltf::Accessor* tangentsAccessor = GetAttributeAccessor(prim, model, tangentsAccessorName))
		{
			SetTangents(*tangentsAccessor, model);
		}

		const static lib::String uvsAccessorName = "TEXCOORD_0";
		if (const tinygltf::Accessor* uvsAccessor = GetAttributeAccessor(prim, model, uvsAccessorName))
		{
			SetUVs(*uvsAccessor, model);
		}
	}
}

const tinygltf::Accessor* GLTFMeshBuilder::GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name)
{
	const auto foundAccessorIdxIt = prim.attributes.find(name);
	const Int32 foundAccessorIdx = foundAccessorIdxIt != std::cend(prim.attributes) ? foundAccessorIdxIt->second : -1;
	return foundAccessorIdx != -1 ? &model.accessors[foundAccessorIdx] : nullptr;
}

void GLTFMeshBuilder::SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.indicesNum == 0);
	SPT_CHECK(submeshBD.submesh.indicesOffset == idxNone<Uint32>);

	submeshBD.submesh.indicesOffset = static_cast<Uint32>(GetCurrentDataSize());
	submeshBD.submesh.indicesNum = AppendAccessorData<Uint32>(accessor, model);
}

void GLTFMeshBuilder::SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.locationsOffset == idxNone<Uint32>);
	
	submeshBD.submesh.locationsOffset = static_cast<Uint32>(GetCurrentDataSize());
	submeshBD.vertexCount = AppendAccessorData<Real32>(accessor, model, { 2, 0, 1 });
}

void GLTFMeshBuilder::SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.normalsOffset == idxNone<Uint32>);
	
	submeshBD.submesh.normalsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model, { 2, 0, 1 });
}

void GLTFMeshBuilder::SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.tangentsOffset == idxNone<Uint32>);
	
	submeshBD.submesh.tangentsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model, { 2, 0, 1, 3 });
}

void GLTFMeshBuilder::SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.uvsOffset == idxNone<Uint32>);
	
	submeshBD.submesh.uvsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

template<typename TDestType>
Uint32 GLTFMeshBuilder::AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model, const lib::DynamicArray<SizeType>& remapping /*= lib::DynamicArray<SizeType>{}*/)
{
	SPT_PROFILER_FUNCTION();

	const Int32 bufferViewIdx = accessor.bufferView;
	SPT_CHECK(bufferViewIdx != -1);

	const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
	const Int32 bufferIdx = bufferView.buffer;
	SPT_CHECK(bufferIdx != -1);

	const tinygltf::Buffer& buffer = model.buffers[bufferIdx];

	const SizeType bufferByteOffset = accessor.byteOffset + bufferView.byteOffset;
	const Int32 strideAsInt = accessor.ByteStride(bufferView);
	SPT_CHECK(strideAsInt != -1);
	const SizeType stride = static_cast<SizeType>(strideAsInt);

	const SizeType elementsNum = accessor.count;
	const SizeType componentsNum = tinygltf::GetNumComponentsInType(accessor.type);
	const Int32 componentType = accessor.componentType;
	
	const unsigned char* data = buffer.data.data() + bufferByteOffset;

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		AppendData<TDestType, char>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		AppendData<TDestType, unsigned char>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		AppendData<TDestType, short>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		AppendData<TDestType, unsigned short>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		AppendData<TDestType, Int32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		AppendData<TDestType, Uint32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		AppendData<TDestType, Real32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		AppendData<TDestType, Real64>(data, componentsNum, stride, elementsNum, remapping);
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	// value must be lower beceuase maxValue is reserved for "invalid"
	SPT_CHECK(elementsNum < maxValue<Uint32>);
	return static_cast<Uint32>(elementsNum);
}

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
		SPT_CHECK(math::Utils::AreNearlyEqual(node.scale[0], node.scale[1]));
		SPT_CHECK(math::Utils::AreNearlyEqual(node.scale[0], node.scale[2]));

		const math::Map<const math::Vector3d> nodeScaleDouble(node.scale.data());
		const math::Vector3f nodeScale = nodeScaleDouble.cast<float>();
		const math::AlignedScaling3f remappedScale(nodeScale.z(), nodeScale.x(), nodeScale.y());
		transform = remappedScale * transform;
	}

	if (!node.rotation.empty())
	{
		SPT_CHECK(node.rotation.size() == 4);
		const math::Vector4f nodeQuaternion = math::Map<const math::Vector4d>(node.rotation.data()).cast<float>();
		const math::Quaternionf remappedQuaternion(math::Vector4f(nodeQuaternion.z(), nodeQuaternion.x(), nodeQuaternion.y(), nodeQuaternion.w()));
		transform = remappedQuaternion.matrix() * transform;
	}
	
	if (!node.translation.empty())
	{
		SPT_CHECK(node.translation.size() == 3);
		const math::Vector3f nodeTranslation = math::Map<const math::Vector3d>(node.translation.data()).cast<float>();
		const math::Translation3f remappedTranslation(nodeTranslation.z(), nodeTranslation.x(), nodeTranslation.y());
		transform = remappedTranslation * transform;
	}

	return transform;
}

static RenderingDataEntityHandle LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	GLTFMeshBuilder builder;
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

static lib::DynamicArray<Uint32> LoadImages(const tinygltf::Model& model)
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

	// Transition barrier to transfer dst
	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Timeline);
	const lib::SharedRef<rdr::Semaphore> readyForTransferSemaphore = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("ReadyForTransferSemaphore"), semaphoreDef);

	{
		lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::Renderer::StartRecordingCommands();

		rhi::RHIDependency preTransferDependency;
		for (const auto& texture : textures)
		{
			const SizeType dependencyIdx = preTransferDependency.AddTextureDependency(texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			preTransferDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::TransferDest);
		}
		commandRecorder->ExecuteBarrier(std::move(preTransferDependency));

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("PreTransferDependencyCmdBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		commandRecorder->RecordCommands(context, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		rdr::CommandsSubmitBatch submitBatch;
		submitBatch.recordedCommands.emplace_back(std::move(commandRecorder));
		submitBatch.signalSemaphores.AddTimelineSemaphore(readyForTransferSemaphore, 1, rhi::EPipelineStage::BottomOfPipe);
		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, std::move(submitBatch));
	}

	readyForTransferSemaphore->GetRHI().Wait(1);

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
		lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::Renderer::StartRecordingCommands();

		for (const auto& texture : textures)
		{
			gfx::GenerateMipMaps(*commandRecorder, texture, rhi::ETextureAspect::Color, 0, rhi::TextureTransition::ReadOnly);
		}

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("PostTransferDependencyCmdBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Default);
		commandRecorder->RecordCommands(context, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		rdr::CommandsSubmitBatch submitBatch;
		submitBatch.recordedCommands.emplace_back(std::move(commandRecorder));
		gfx::TransfersManager::WaitForTransfersFinished(submitBatch.waitSemaphores);
		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, std::move(submitBatch));
	}

	lib::DynamicArray<Uint32> textureIndicesInMaterialDS;
	textureIndicesInMaterialDS.reserve(textures.size());

	for (const auto& texture : textures)
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange.aspect = rhi::ETextureAspect::Color;
		lib::SharedRef<rdr::TextureView> textureView = texture->CreateView(RENDERER_RESOURCE_NAME(texture->GetRHI().GetName().ToString() + "View"), viewDef);
		textureIndicesInMaterialDS.emplace_back(MaterialsUnifiedData::Get().AddMaterialTexture(std::move(textureView)));
	}

	return textureIndicesInMaterialDS;
}

static lib::DynamicArray<RenderingDataEntityHandle> CreateMaterials(const tinygltf::Model& model, const lib::DynamicArray<Uint32>& textureIndicesInMaterialDS)
{
	const auto getLoadedTextureIndex = [&model, &textureIndicesInMaterialDS](int modelTextureIdx)
	{
		Uint32 loadedTextureIdx = idxNone<Uint32>;
		if (modelTextureIdx != -1)
		{
			const SizeType imageIdx = model.textures[modelTextureIdx].source;
			loadedTextureIdx = static_cast<Uint32>(textureIndicesInMaterialDS[imageIdx]);
		}
		return loadedTextureIdx;
	};

	lib::DynamicArray<RenderingDataEntityHandle> materials;
	materials.reserve(model.materials.size());

	for (const tinygltf::Material& materialDef : model.materials)
	{
		const RenderingDataEntityHandle materialDataHandle = RenderingData::CreateEntity();

		const tinygltf::PbrMetallicRoughness& pbrDef = materialDef.pbrMetallicRoughness;

		MaterialPBRData pbrData;
		pbrData.baseColorFactor				= math::Map<const math::Vector3d>(pbrDef.baseColorFactor.data()).cast<Real32>();
		pbrData.metallicFactor				= static_cast<Real32>(pbrDef.metallicFactor);
		pbrData.roughnessFactor				= static_cast<Real32>(pbrDef.roughnessFactor);
		pbrData.baseColorTextureIdx			= getLoadedTextureIndex(pbrDef.baseColorTexture.index);
		pbrData.metallicRoughnessTextureIdx	= getLoadedTextureIndex(pbrDef.metallicRoughnessTexture.index);
		pbrData.normalsTextureIdx			= getLoadedTextureIndex(materialDef.normalTexture.index);

		const rhi::RHISuballocation materialDataSuballocation = MaterialsUnifiedData::Get().CreateMaterialDataSuballocation(reinterpret_cast<const Byte*>(&pbrData), sizeof(MaterialPBRData));

		materialDataHandle.emplace<MaterialCommonData>(MaterialCommonData{ materialDataSuballocation });

		materials.emplace_back(materialDataHandle);
	}

	return materials;
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
		const lib::DynamicArray<Uint32> textureIndicesInMaterialDS = LoadImages(model);
		const lib::DynamicArray<RenderingDataEntityHandle> materials = CreateMaterials(model, textureIndicesInMaterialDS);

		lib::DynamicArray<RenderingDataEntityHandle> loadedMeshes;
		loadedMeshes.reserve(model.meshes.size());

		for (const tinygltf::Mesh& mesh : model.meshes)
		{
			loadedMeshes.emplace_back(LoadMesh(mesh, model));
		}

		for (const tinygltf::Node& node : model.nodes)
		{
			if (node.mesh != -1)
			{
				SPT_CHECK(static_cast<SizeType>(node.mesh) < loadedMeshes.size());

				const tinygltf::Mesh& mesh = model.meshes[node.mesh];

				RenderInstanceData instanceData;
				instanceData.transformComp.transform = GetNodeTransform(node);
				const RenderSceneEntityHandle meshSceneEntity = scene.CreateEntity(instanceData);

				StaticMeshInstanceRenderData entityStaticMeshData;
				entityStaticMeshData.staticMesh = loadedMeshes[node.mesh];

				entityStaticMeshData.materials.reserve(mesh.primitives.size());
				for (const tinygltf::Primitive& prim : mesh.primitives)
				{
					entityStaticMeshData.materials.emplace_back(materials[prim.material]);
				}

				meshSceneEntity.emplace<StaticMeshInstanceRenderData>(entityStaticMeshData);
			}
		}
	}
}

} // glTFLoader

} // spt::rsc
