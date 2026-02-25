#include "GlobalResourcesRegistry.h"
#include "GPUApi.h"
#include "JobSystem.h"
#include "Loaders/TextureLoader.h"
#include "Paths.h"


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
	const lib::String texturePath = engn::Paths::Combine(engn::Paths::GetContentPath(), m_path);
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

GlobalResourcesRegistry& GlobalResourcesRegistry::Get()
{
	static GlobalResourcesRegistry instance;
	return instance;
}

void GlobalResourcesRegistry::RegisterGlobalResource(GlobalResource* resource)
{
	m_resources.emplace_back(resource);
}

void GlobalResourcesRegistry::InitializeAll()
{
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
