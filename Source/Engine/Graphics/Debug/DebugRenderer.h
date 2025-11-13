#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "GraphicsMacros.h"


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


BEGIN_SHADER_STRUCT(DebugMarkerDefinition)
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32,         size)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
END_SHADER_STRUCT()


BEGIN_SHADER_STRUCT(DebugSphereDefinition)
	SHADER_STRUCT_FIELD(math::Vector3f, center)
	SHADER_STRUCT_FIELD(Real32,         radius)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
END_SHADER_STRUCT()


BEGIN_SHADER_STRUCT(GPUDebugRendererData)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<DebugLineDefinition>, rwLines)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<Uint32>,              rwLinesNum)

	SHADER_STRUCT_FIELD(RWTypedBufferRef<DebugMarkerDefinition>, rwMarkers)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<Uint32>,                rwMarkersNum)

	SHADER_STRUCT_FIELD(RWTypedBufferRef<DebugSphereDefinition>, rwSpheres)
	SHADER_STRUCT_FIELD(RWTypedBufferRef<Uint32>,                rwSpheresNum)
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


struct GPUDebugGeometryData
{
	lib::SharedPtr<rdr::Buffer> geometries;
	lib::SharedPtr<rdr::Buffer> drawCall;

	lib::SharedPtr<rdr::BindableBufferView> geometriesNumView;

	Uint32 verticesNum = 0u;
};


class GRAPHICS_API DebugRenderer
{
public:

	explicit DebugRenderer(lib::HashedString name);

	void PrepareResourcesForRecording(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingFrameSettings& frameSettings);

	GPUDebugRendererData GetGPUDebugRendererData() const;

	void RenderDebugGeometry(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingSettings& settings);

private:

	lib::HashedString m_name;

	GPUDebugGeometryData m_linesData;
	GPUDebugGeometryData m_markersData;
	GPUDebugGeometryData m_spheresData;

	lib::SharedPtr<rdr::Buffer> m_sphereVerticesBuffer;

	Bool m_initialized = false;
};

} // spt::gfx