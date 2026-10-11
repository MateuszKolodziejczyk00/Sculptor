#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Plugins/Plugin.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


#define SPT_GLOBAL_SHADER_RESOURCES(X) \
	X(gfx::ConstSRVTexture2D<Real32>, blueNoise256,          "Textures/Bluenoise256.png")        \
	X(gfx::ConstSRVTexture2D<Real32>, MS_GGX_E_avg_LUT,      "Textures/GGX_EAvg_LUT.dds")        \
	X(gfx::ConstSRVTexture2D<Real32>, MS_GGX_E_LUT,          "Textures/GGX_E_LUT.dds")           \
	X(gfx::ConstSRVTexture3D<Real32>, MS_GGX_THROUGHPUT_LUT, "Textures/GGX_Throughput_LUT.dds")


namespace spt::gfx
{

#define SPT_GLOBAL_SHADER_RESOURCE_FIELD(type, name, path) SHADER_STRUCT_FIELD(type, name)

BEGIN_SHADER_STRUCT(GlobalShaderResources)
	SPT_GLOBAL_SHADER_RESOURCES(SPT_GLOBAL_SHADER_RESOURCE_FIELD)
END_SHADER_STRUCT();

#undef SPT_GLOBAL_SHADER_RESOURCE_FIELD


class GRAPHICS_API GlobalShaderResourcesManager : public engn::Plugin
{
	using Super = engn::Plugin;

public:

	GlobalShaderResourcesManager() = default;

	static GlobalShaderResourcesManager& Get();

	const GlobalShaderResources& GetResources() const { return m_resources; }

	void UpdateResources(const GlobalShaderResources& newResources);

protected:

	// Begin Plugin overrides
	virtual void OnPostEngineInit() override;
	// End Plugin overrides

private:

	void InitializeResources();
	void ReleaseResources();

	GlobalShaderResources m_resources;

	lib::SharedPtr<rdr::Buffer> m_resourcesBuffer;
};

} // spt::gfx
