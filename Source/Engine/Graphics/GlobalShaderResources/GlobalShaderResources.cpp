#include "GlobalShaderResources.h"
#include "Engine.h"
#include "GPUApi.h"
#include "ResourcesManager.h"
#include "Types/Buffer.h"
#include "Types/Texture.h"
#include "Loaders/TextureLoader.h"
#include "Utils/TransfersUtils.h"


SPT_DEFINE_LOG_CATEGORY(LogGlobalShaderResources, true);


namespace spt::gfx
{

SPT_DEFINE_PLUGIN(GlobalShaderResourcesManager);


namespace global_shader_resources
{

template<TextureDescriptorMetadata metadata, typename TType>
static void LoadResource(TextureDescriptor<metadata, TType>& descriptor, const char* name, const char* contentRelativePath)
{
	static_assert(metadata.isConst && metadata.isOptional, "Global shader textures must be const and optional");

	const lib::Path path = engn::GetEngine().GetPaths().contentPath / contentRelativePath;

	lib::SharedPtr<rdr::Texture> texture;
	if (std::filesystem::exists(path))
	{
		texture = TextureLoader::LoadTexture(path.generic_string());
	}

	if (!texture)
	{
		SPT_LOG_WARN(LogGlobalShaderResources, "Failed to load global shader resource '{}' from '{}'. Resource will be unbound", name, path.generic_string());
		return;
	}

	const lib::SharedPtr<rdr::TextureView> textureView = texture->CreateView(RENDERER_RESOURCE_NAME_FORMATTED("Global {}", name));
	descriptor = textureView;
}

} // global_shader_resources


GlobalShaderResourcesManager& GlobalShaderResourcesManager::Get()
{
	return engn::GetEngine().GetPluginsManager().GetPluginChecked<GlobalShaderResourcesManager>();
}

void GlobalShaderResourcesManager::UpdateResources(const GlobalShaderResources& newResources)
{
	SPT_PROFILER_FUNCTION();

	m_resources = newResources;

	const rdr::HLSLStorage<GlobalShaderResources> hlslResources = m_resources;
	const lib::Span<const Byte> hlslData = hlslResources.GetHLSLDataSpan();

	rdr::UploadDataToBuffer(lib::Ref(m_resourcesBuffer), 0u, hlslData.data(), hlslData.size());

	rdr::FlushPendingUploads();
}

void GlobalShaderResourcesManager::OnPostEngineInit()
{
	Super::OnPostEngineInit();

	rdr::GPUApi::GetOnRendererInitializedDelegate().AddRawMember(this, &GlobalShaderResourcesManager::InitializeResources);
}

void GlobalShaderResourcesManager::InitializeResources()
{
	SPT_PROFILER_FUNCTION();

#define SPT_LOAD_GLOBAL_SHADER_RESOURCE(type, name, path) global_shader_resources::LoadResource(m_resources.name, #name, path);
	SPT_GLOBAL_SHADER_RESOURCES(SPT_LOAD_GLOBAL_SHADER_RESOURCE)
#undef SPT_LOAD_GLOBAL_SHADER_RESOURCE

	const rdr::HLSLStorage<GlobalShaderResources> hlslResources = m_resources;
	const lib::Span<const Byte> hlslData = hlslResources.GetHLSLDataSpan();

	const rhi::BufferDefinition bufferDef(hlslData.size(), (rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	m_resourcesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Global Shader Resources"), bufferDef, rhi::EMemoryUsage::GPUOnly);
	rdr::UploadDataToBuffer(lib::Ref(m_resourcesBuffer), 0u, hlslData.data(), hlslData.size());

	rdr::GPUApi::SetGlobalShaderParam(rdr::GlobalShaderParam{ GlobalShaderResources::GetStructName(), m_resourcesBuffer->GetRHI().GetDeviceAddress() });

	rdr::FlushPendingUploads();

	rdr::GPUApi::GetOnRendererCleanupDelegate().AddRawMember(this, &GlobalShaderResourcesManager::ReleaseResources);
}

void GlobalShaderResourcesManager::ReleaseResources()
{
	rdr::GPUApi::SetGlobalShaderParam(rdr::GlobalShaderParam{});

	m_resources = GlobalShaderResources{};
	m_resourcesBuffer.reset();
}

} // spt::gfx
