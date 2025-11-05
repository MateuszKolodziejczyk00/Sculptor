#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::gfx
{

class ViewRenderingSpec;


BEGIN_SHADER_STRUCT(DebugLineDefinition)
	SHADER_STRUCT_FIELD(math::Vector3f, begin)
	SHADER_STRUCT_FIELD(math::Vector3f, end)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
END_SHADER_STRUCT()


BEGIN_SHADER_STRUCT(GPUDebugRendererData)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<DebugLineDefinition>, rwLines)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<Uint32>,              rwLinesNum)
END_SHADER_STRUCT()


struct DebugRenderingSettings
{
	rg::RGTextureViewHandle outDepth;
	rg::RGTextureViewHandle outColor;

	math::Matrix4f viewProjectionMatrix;

	Bool renderLines = true;
};


struct DebugRenderingFrameSettings
{
	Bool clearGeometry = true;
};


class DebugRenderer
{
public:

	explicit DebugRenderer(lib::HashedString name);

	void PrepareResourcesForRecording(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingFrameSettings& frameSettings);

	GPUDebugRendererData GetGPUDebugRendererData() const;

	void RenderDebugGeometry(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingSettings& settings);

private:

	lib::HashedString m_name;

	lib::SharedPtr<rdr::Buffer> m_gpuLines;
	lib::SharedPtr<rdr::Buffer> m_gpuLinesDrawCall;

	lib::SharedPtr<rdr::BindableBufferView> m_gpuLinesNumView;

	Bool m_initialized = false;
};

} // spt::gfx