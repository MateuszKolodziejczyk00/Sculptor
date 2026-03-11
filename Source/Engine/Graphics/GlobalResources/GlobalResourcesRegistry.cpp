#include "GlobalResourcesRegistry.h"
#include "GPUApi.h"
#include "JobSystem.h"
#include "Loaders/TextureLoader.h"
#include "Engine.h"


namespace spt::gfx::global
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// GlobalResource ================================================================================

GlobalResource::GlobalResource()
{
	GlobalResourcesRegistry::Get().RegisterGlobalResource(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GlobalTexture =================================================================================

GlobalTexture::GlobalTexture(const char* path)
	: m_path(path)
{
}

void GlobalTexture::Initialize()
{
	const lib::String texturePath = (engn::GetEngine().GetPaths().contentPath / m_path).generic_string();
	m_texture = gfx::TextureLoader::LoadTexture(texturePath, lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest));
	SPT_CHECK(!!m_texture);
	m_textureView = m_texture->CreateView(RENDERER_RESOURCE_NAME_FORMATTED("{} View", m_texture->GetRHI().GetName().GetData()));
}

void GlobalTexture::Release()
{
	m_textureView.reset();
	m_texture.reset();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GlobalResourcesRegistry =======================================================================

engn::TEngineSingleton<GlobalResourcesRegistry> resourcesRegistry;

GlobalResourcesRegistry& GlobalResourcesRegistry::Get()
{
	return resourcesRegistry.Get();
}

void GlobalResourcesRegistry::RegisterGlobalResource(GlobalResource* resource)
{
	m_resources.emplace_back(resource);
}

void GlobalResourcesRegistry::InitializeAll()
{
	// Make sure that resources instances are created
	Resources::Get();

	js::InlineParallelForEach(SPT_GENERIC_JOB_NAME,
							  m_resources,
							  [](GlobalResource* resource)
							  {
								  resource->Initialize();
							  });

	rdr::GPUApi::GetOnRendererCleanupDelegate().AddRawMember(this, &GlobalResourcesRegistry::ReleaseAll);
}

void GlobalResourcesRegistry::ReleaseAll()
{
	js::InlineParallelForEach(SPT_GENERIC_JOB_NAME,
							  m_resources,
							  [](GlobalResource* resource)
							  {
								  resource->Release();
							  });

	m_resources.clear();
}

} // spt::gfx::global
