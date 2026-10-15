#pragma once

#include "SculptorCoreTypes.h"
#include "Bindless/BindlessTypes.h"
#include "RenderGraphBuilder.h"
#include "Logging/LogMacros.h"


SPT_DEFINE_LOG_CATEGORY(ClosureViewer, true);


namespace spt::closure_viewer
{

BEGIN_SHADER_STRUCT(ClosureMaterial)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColor)
	SHADER_STRUCT_FIELD(Real32,         metallic)
	SHADER_STRUCT_FIELD(Real32,         roughness)
	SHADER_STRUCT_FIELD(Real32,         clearCoatWeight)
	SHADER_STRUCT_FIELD(Real32,         clearCoatRoughness)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ClosureViewerData)
	SHADER_STRUCT_FIELD(ClosureMaterial,                  material)
	SHADER_STRUCT_FIELD(math::Matrix4f,                   viewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Vector3f,                   viewDirection)
	SHADER_STRUCT_FIELD(Real32,                           closureScale)
	SHADER_STRUCT_FIELD(Real32,                           closureOpacity)
	SHADER_STRUCT_FIELD(Uint32,                           frameIdx)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<math::Vector3f>, closure3DVertices)
	SHADER_STRUCT_FIELD(Uint32,                           closure3DVerticesNum)
END_SHADER_STRUCT();


namespace renderer
{

namespace utils
{

lib::DynamicArray<math::Vector3f> GenerateDebugSphereVertices(Uint32 latitudeSegments, Uint32 longitudeSegments);

math::Matrix4f CreatePerspectiveProjection(Real32 fovRadians, const math::Vector2f& resolution, Real32 near);

math::Matrix4f CreateLookAtMatrix(const math::Vector3f& location, const math::Vector3f& target);

} // utils

void RenderFurnanceTest(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData, Uint32 samplesNum = 1u);

constexpr rhi::EFragmentFormat closure3DColorFormat = rhi::EFragmentFormat::RGBA8_UN_Float;
void RenderClosure3D(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle color, const ClosureViewerData& viewerData);

void RenderClosureGraph(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData);

void RenderClosure2D(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData);

} // renderer

} // spt::closure_viewer
