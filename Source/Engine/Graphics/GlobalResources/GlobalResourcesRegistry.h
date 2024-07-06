#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Texture.h"


namespace spt::gfx::global
{

class GRAPHICS_API GlobalResource
{
public:

	GlobalResource();

	virtual void Initialize() = 0;
	virtual void Release() = 0;
};


class GRAPHICS_API GlobalTexture : public GlobalResource
{

public:

	explicit GlobalTexture(const char* path);

	// Begin GlobalResource interface
	virtual void Initialize() override;
	virtual void Release() override;
	//	End GlobalResource interface

	const lib::SharedPtr<rdr::Texture>& GetTexture()    const { return m_texture; } 
	lib::SharedRef<rdr::Texture>        GetTextureRef() const { return lib::Ref(m_texture); }

	const lib::SharedPtr<rdr::TextureView>& GetView()    const { return m_textureView; } 
	lib::SharedRef<rdr::TextureView>        GetViewRef() const { return lib::Ref(m_textureView); }

private:

	lib::SharedPtr<rdr::Texture>     m_texture;
	lib::SharedPtr<rdr::TextureView> m_textureView;

	const char* m_path;
};


class GRAPHICS_API GlobalResourcesRegistry
{
public:

	static GlobalResourcesRegistry& Get();

	void RegisterGlobalResource(GlobalResource* resource);

	void InitializeAll();

private:

	GlobalResourcesRegistry() = default;

	void ReleaseAll();

	lib::DynamicArray<GlobalResource*> m_resources;
};

} // spt::gfx::global