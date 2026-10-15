#include "ClosureViewerCommon.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Types/Buffer.h"


namespace spt::closure_viewer::renderer
{

namespace utils
{

lib::DynamicArray<math::Vector3f> GenerateDebugSphereVertices(Uint32 latitudeSegments, Uint32 longitudeSegments)
{
	SPT_PROFILER_FUNCTION();

	const float rcpLatitudeSegments  = 1.0f / static_cast<Real32>(latitudeSegments);
	const float rcpLongitudeSegments = 1.0f / static_cast<Real32>(longitudeSegments);

	const auto ComputeVertex = [=](Uint32 lat, Uint32 lon) -> math::Vector3f
	{
		const Real32 theta    = static_cast<Real32>(lat) * pi<Real32> * rcpLatitudeSegments;
		const Real32 phi      = static_cast<Real32>(lon) * 2.0f * pi<Real32> * rcpLongitudeSegments;
		const Real32 sinTheta = std::sin(theta);
		return math::Vector3f(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, std::cos(theta));
	};

	lib::DynamicArray<math::Vector3f> vertices;
	vertices.reserve(latitudeSegments * longitudeSegments * 6u);

	for (Uint32 lat = 0; lat < latitudeSegments; ++lat)
	{
		for (Uint32 lon = 0; lon < longitudeSegments; ++lon)
		{
			const math::Vector3f v00 = ComputeVertex(lat,      lon);
			const math::Vector3f v10 = ComputeVertex(lat + 1u, lon);
			const math::Vector3f v01 = ComputeVertex(lat,      lon + 1u);
			const math::Vector3f v11 = ComputeVertex(lat + 1u, lon + 1u);

			if (lat != 0u)
			{
				vertices.emplace_back(v00);
				vertices.emplace_back(v10);
				vertices.emplace_back(v01);
			}

			if (lat != latitudeSegments - 1u)
			{
				vertices.emplace_back(v10);
				vertices.emplace_back(v11);
				vertices.emplace_back(v01);
			}
		}
	}

	return vertices;
}

math::Matrix4f CreatePerspectiveProjection(Real32 fovRadians, const math::Vector2f& resolution, Real32 near)
{
	const Real32 h = 1.f / std::tan(fovRadians * 0.5f);

	const Real32 minDimension = resolution.minCoeff();

	const Real32 a = h * minDimension / resolution.x();
	const Real32 b = -h * minDimension / resolution.y();

	return math::Matrix4f{ { 0.f, a,   0.f, 0.f  },
						   { 0.f, 0.f, b,   0.f  },
						   { 0.f, 0.f, 0.f, near },
						   { 1.f, 0.f, 0.f, 0.f  } };
}

math::Matrix4f CreateLookAtMatrix(const math::Vector3f& location, const math::Vector3f& target)
{
	const math::Vector3f forward = (target - location).normalized();
	const math::Vector3f right   = math::Vector3f::UnitZ().cross(forward).normalized();
	const math::Vector3f up      = forward.cross(right);

	math::Matrix4f viewMatrix = math::Matrix4f::Identity();
	viewMatrix.block<1, 3>(0, 0) = forward.transpose();
	viewMatrix.block<1, 3>(1, 0) = right.transpose();
	viewMatrix.block<1, 3>(2, 0) = up.transpose();
	viewMatrix(0, 3) = -forward.dot(location);
	viewMatrix(1, 3) = -right.dot(location);
	viewMatrix(2, 3) = -up.dot(location);

	return viewMatrix;
}

} // utils


SIMPLE_COMPUTE_PSO(ClosureFurnanceTestPSO, "Sculptor/Tools/ClosureViewer/ClosureFurnanceTest.hlsl", ClosureFurnanceTestCS);


BEGIN_SHADER_STRUCT(ClosureFurnanceTestConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
	SHADER_STRUCT_FIELD(Uint32,                            samplesNum)
END_SHADER_STRUCT();


void RenderFurnanceTest(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData, Uint32 samplesNum /* = 1u */)
{
	SPT_PROFILER_FUNCTION();

	ClosureFurnanceTestConstants shaderConstants;
	shaderConstants.rwOutput   = output;
	shaderConstants.samplesNum = samplesNum;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Furnance Test"),
						  ClosureFurnanceTestPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}


namespace closure_3d_priv
{

constexpr rhi::EFragmentFormat depthFormat = rhi::EFragmentFormat::D32_S_Float;

constexpr Uint32 groundGridVerticesNum     = 6u;
constexpr Uint32 directionLinesVerticesNum = 6u;


rhi::GraphicsPipelineDefinition CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology topology)
{
	rhi::GraphicsPipelineDefinition pipelineDefinition;
	pipelineDefinition.primitiveTopology                = topology;
	pipelineDefinition.rasterizationDefinition.cullMode = rhi::ECullMode::None;
	pipelineDefinition.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition{ .format = closure3DColorFormat });
	pipelineDefinition.renderTargetsDefinition.depthRTDefinition.format = depthFormat;
	return pipelineDefinition;
}

} // closure_3d_priv


GRAPHICS_PSO(ClosureGroundGridPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   GroundGridVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", GroundGridFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, closure_3d_priv::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::TriangleList), {});
	}
};


GRAPHICS_PSO(ClosureDirectionLinesPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   DirectionLinesVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", DirectionLinesFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, closure_3d_priv::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::LineList), {});
	}
};


GRAPHICS_PSO(Closure3DPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   Closure3DVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", Closure3DFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition pipelineDefinition = closure_3d_priv::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::TriangleList);

		rhi::ColorRenderTargetDefinition& colorRTDef = pipelineDefinition.renderTargetsDefinition.colorRTsDefinition[0];
		colorRTDef.colorBlendType = rhi::ERenderTargetBlendType::AlphaBlend;
		colorRTDef.alphaBlendType = rhi::ERenderTargetBlendType::AlphaBlend;
		pipelineDefinition.renderTargetsDefinition.depthRTDefinition.enableDepthWrite = false;

		pso = CompilePSO(compiler, pipelineDefinition, {});
	}
};


void RenderClosure3D(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle color, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(color->GetFormat() == closure3DColorFormat);

	const math::Vector2u resolution = color->GetResolution2D();

	const rg::RGTextureViewHandle depth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Closure 3D Depth"), rg::TextureDef(resolution, closure_3d_priv::depthFormat));

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);

	rg::RGRenderTargetDef colorRTDef;
	colorRTDef.textureView   = color;
	colorRTDef.loadOperation = rhi::ERTLoadOperation::Clear;
	colorRTDef.clearColor    = rhi::ClearColor(0.05f, 0.05f, 0.06f, 1.f);
	renderPassDef.AddColorRenderTarget(colorRTDef);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView   = depth;
	depthRTDef.loadOperation = rhi::ERTLoadOperation::Clear;
	depthRTDef.clearColor    = rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	const Uint32 closureVerticesNum = viewerData.closure3DVerticesNum;

	graphBuilder.RenderPass(RG_DEBUG_NAME("Render Closure 3D"),
							renderPassDef,
							rg::ShaderParams(viewerData),
							[resolution, closureVerticesNum](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								recorder.BindGraphicsPipeline(ClosureGroundGridPSO::pso);
								recorder.DrawInstances(closure_3d_priv::groundGridVerticesNum, 1u);

								recorder.BindGraphicsPipeline(ClosureDirectionLinesPSO::pso);
								recorder.DrawInstances(closure_3d_priv::directionLinesVerticesNum, 1u);

								recorder.BindGraphicsPipeline(Closure3DPSO::pso);
								recorder.DrawInstances(closureVerticesNum, 1u);
							});
}


SIMPLE_COMPUTE_PSO(ClosureGraphPSO, "Sculptor/Tools/ClosureViewer/ClosureGraph.hlsl", ClosureGraphCS);


BEGIN_SHADER_STRUCT(ClosureGraphConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT();


void RenderClosureGraph(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	ClosureGraphConstants shaderConstants;
	shaderConstants.rwOutput = output;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Closure Graph"),
						  ClosureGraphPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}



SIMPLE_COMPUTE_PSO(Closure2DPSO, "Sculptor/Tools/ClosureViewer/Closure2D.hlsl", Closure2DCS);


BEGIN_SHADER_STRUCT(Closure2DConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT();


void RenderClosure2D(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	Closure2DConstants shaderConstants;
	shaderConstants.rwOutput = output;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Closure 2D"),
						  Closure2DPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}

} // spt::closure_viewer::renderer
