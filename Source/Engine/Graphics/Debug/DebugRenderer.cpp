#include "DebugRenderer.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RGDescriptorSetState.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Transfers/UploadsManager.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(DebugDrawCallData)
	/* API Command data */
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	/* Custom Data */
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(DebugGeometryIndirectDrawCommandsParameters)
	RG_BUFFER_VIEW(linesDrawCall,   rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(markersDrawCall, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(spheresDrawCall, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


BEGIN_SHADER_STRUCT(DebugGeometryPassConstants)
	SHADER_STRUCT_FIELD(math::Matrix4f,                        viewProjectionMatrix)
	SHADER_STRUCT_FIELD(TypedBufferRef<DebugLineDefinition>,   lines)
	SHADER_STRUCT_FIELD(TypedBufferRef<DebugMarkerDefinition>, markers)
	SHADER_STRUCT_FIELD(TypedBufferRef<DebugSphereDefinition>, spheres)
	SHADER_STRUCT_FIELD(ConstTypedBufferRef<math::Vector3f>,   sphereVertices)
END_SHADER_STRUCT();


DS_BEGIN(DebugGeometryPassDS, rg::RGDescriptorSetState<DebugGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(ConstantBufferBinding<DebugGeometryPassConstants>), u_passConstants)
DS_END();


GRAPHICS_PSO(DebugGeometryPSO)
{
	VERTEX_SHADER("Sculptor/Debug/DebugGeometry.hlsl", DebugGeometryVS);
	FRAGMENT_SHADER("Sculptor/Debug/DebugGeometry.hlsl", DebugGeometryPS);

	PRESET(lines);
	PRESET(markers);
	PRESET(spheres);

	static void PrecachePSOs(rdr::PSOCompilerInterface & compiler, const rdr::PSOPrecacheParams & params)
	{
		const rhi::PipelineRenderTargetsDefinition rtsDef
		{
			.colorRTsDefinition =
			{
				rhi::ColorRenderTargetDefinition
				{
					.format = rhi::EFragmentFormat::RGBA8_UN_Float,
				}
			},
			.depthRTDefinition =
			{
				.format = rhi::EFragmentFormat::D32_S_Float,
			}
		};

		lines = CompilePSO(compiler,
						   rhi::GraphicsPipelineDefinition
						   {
							   .primitiveTopology = rhi::EPrimitiveTopology::LineList,
							   .renderTargetsDefinition = rtsDef,
						   },
						   { "DEBUG_GEOMETRY_TYPE=0" });

		markers = CompilePSO(compiler,
							 rhi::GraphicsPipelineDefinition
							 {
								 .primitiveTopology = rhi::EPrimitiveTopology::LineList,
								 .renderTargetsDefinition = rtsDef,
							 },
							 { "DEBUG_GEOMETRY_TYPE=1" });

		spheres = CompilePSO(compiler,
							 rhi::GraphicsPipelineDefinition
							 {
								 .primitiveTopology = rhi::EPrimitiveTopology::LineList,
								 .renderTargetsDefinition = rtsDef,
							 },
							 { "DEBUG_GEOMETRY_TYPE=2" });
	}
};


namespace debug_utils
{

template<typename TDebugGeometry>
GPUDebugGeometryData CreateDebugGeometryData(Uint32 verticesNum, Uint32 maxNum)
{
	GPUDebugGeometryData data;

	SPT_MAYBE_UNUSED
	const char* debugGeometryName = TDebugGeometry::GetStructName();

	const rhi::BufferDefinition geometriesBufferDef(sizeof(rdr::HLSLStorage<TDebugGeometry>) * maxNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	data.geometries = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME_FORMATTED("GPU Debug {}", debugGeometryName), geometriesBufferDef, rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition geometriesDrawCallBufferDef(sizeof(rdr::HLSLStorage<DebugDrawCallData>), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Indirect));
	data.drawCall           = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME_FORMATTED("GPU Debug {} Draw Call", debugGeometryName), geometriesDrawCallBufferDef, rhi::EMemoryUsage::GPUOnly);
	data.geometriesNumView  = data.drawCall->CreateView(sizeof(Uint32), sizeof(Uint32));

	data.verticesNum = verticesNum;

	return data;
}

void ClearDebugGeometry(rg::RenderGraphBuilder& graphBuilder, const GPUDebugGeometryData& geometryData)
{
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Debug Geometry Num"), graphBuilder.AcquireExternalBufferView(geometryData.drawCall->GetFullView()), 0u);
	// Set vertex count
	graphBuilder.FillBuffer(RG_DEBUG_NAME("Clear Debug Geometry Instances Count"), graphBuilder.AcquireExternalBufferView(geometryData.drawCall->GetFullView()), 0u, sizeof(Uint32), geometryData.verticesNum);
}

lib::DynamicArray<math::Vector3f> GenerateDebugSphereVertices(Uint32 latitudeSegments, Uint32 longitudeSegments)
{
	SPT_PROFILER_FUNCTION();

	const float rcpLatitudeSegments  = 1.0f / static_cast<Real32>(latitudeSegments);
	const float rcpLongitudeSegments = 1.0f / static_cast<Real32>(longitudeSegments);

	lib::DynamicArray<math::Vector3f> vertices;

	// Longitude rings
	for (Uint32 lon = 0; lon <= longitudeSegments; ++lon)
	{
		const Real32 phi = static_cast<Real32>(lon) * 2.0f * pi<Real32> * rcpLongitudeSegments;
		const Real32 sinPhi = std::sin(phi);
		const Real32 cosPhi = std::cos(phi);

		vertices.emplace_back(0.f, 0.f, 1.f);
		for (Uint32 lat = 0; lat <= latitudeSegments; ++lat)
		{
			const Real32 theta = static_cast<Real32>(lat) * pi<Real32> * rcpLatitudeSegments;
			const Real32 sinTheta = std::sin(theta);
			const Real32 cosTheta = std::cos(theta);
			const Real32 x = cosPhi * sinTheta;
			const Real32 y = sinPhi * sinTheta;
			const Real32 z = cosTheta;
			vertices.emplace_back(x, y, z); // end of last line
			vertices.emplace_back(x, y, z); // start of new line
		}
		vertices.emplace_back(0.f, 0.f, -1.f);
	}
	
	// ring around equator
	vertices.emplace_back(1.f, 0.f, 0.f);
	for (Uint32 lon = 0; lon <= longitudeSegments; ++lon)
	{
		const Real32 phi = static_cast<Real32>(lon) * 2.0f * pi<Real32> * rcpLongitudeSegments;
		const Real32 sinPhi = std::sin(phi);
		const Real32 cosPhi = std::cos(phi);
		const Real32 x = cosPhi;
		const Real32 y = sinPhi;
		const Real32 z = 0.f;
		vertices.emplace_back(x, y, z); // end of last line
		vertices.emplace_back(x, y, z); // start of new line
	}
	vertices.emplace_back(1.f, 0.f, 0.f);

	return vertices;
}

} // debug_utils

DebugRenderer::DebugRenderer(lib::HashedString name)
	: m_name(std::move(name))
{
	const lib::DynamicArray<math::Vector3f> sphereVertices = debug_utils::GenerateDebugSphereVertices(8u, 12u);
	const rhi::BufferDefinition sphereVerticesBufferDef(sizeof(math::Vector3f) * sphereVertices.size(), lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage));
	m_sphereVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME_FORMATTED("Debug Sphere Vertices"), sphereVerticesBufferDef, rhi::EMemoryUsage::GPUOnly);
	gfx::UploadsManager::Get().EnqueueUpload(lib::Ref(m_sphereVerticesBuffer), 0u, reinterpret_cast<const Byte*>(sphereVertices.data()), sizeof(math::Vector3f) * sphereVertices.size());

	m_linesData   = debug_utils::CreateDebugGeometryData<DebugLineDefinition>(2u, 4096u);
	m_markersData = debug_utils::CreateDebugGeometryData<DebugMarkerDefinition>(6u, 4096u);
	m_spheresData = debug_utils::CreateDebugGeometryData<DebugSphereDefinition>(static_cast<Uint32>(sphereVertices.size()), 4096u);
}

void DebugRenderer::PrepareResourcesForRecording(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingFrameSettings& frameSettings)
{
	SPT_PROFILER_FUNCTION();

	if (frameSettings.clearGeometry || !m_initialized)
	{
		debug_utils::ClearDebugGeometry(graphBuilder, m_linesData);
		debug_utils::ClearDebugGeometry(graphBuilder, m_markersData);
		debug_utils::ClearDebugGeometry(graphBuilder, m_spheresData);

		m_initialized = true;
	}
}

GPUDebugRendererData DebugRenderer::GetGPUDebugRendererData() const
{
	GPUDebugRendererData gpuData;
	gpuData.rwLines      = m_linesData.geometries->GetFullView();
	gpuData.rwLinesNum   = m_linesData.geometriesNumView;
	gpuData.rwMarkers    = m_markersData.geometries->GetFullView();
	gpuData.rwMarkersNum = m_markersData.geometriesNumView;
	gpuData.rwSpheres    = m_spheresData.geometries->GetFullView();
	gpuData.rwSpheresNum = m_spheresData.geometriesNumView;
	return gpuData;
}

void DebugRenderer::RenderDebugGeometry(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(settings.outColor.IsValid());
	SPT_CHECK(settings.outDepth.IsValid());

	const math::Vector2u resolution = settings.outColor->GetResolution2D();
	SPT_CHECK(resolution == settings.outDepth->GetResolution2D());

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);

	rg::RGRenderTargetDef colorTargetDef;
	colorTargetDef.textureView    = settings.outColor;
	colorTargetDef.loadOperation  = rhi::ERTLoadOperation::Load;
	colorTargetDef.storeOperation = rhi::ERTStoreOperation::Store;
	colorTargetDef.clearColor     = rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(colorTargetDef);

	rg::RGRenderTargetDef depthTargetDef;
	depthTargetDef.textureView    = settings.outDepth;
	depthTargetDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthTargetDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthTargetDef);

	DebugGeometryIndirectDrawCommandsParameters indirectParams;
	indirectParams.linesDrawCall   = graphBuilder.AcquireExternalBufferView(m_linesData.drawCall->GetFullView());
	indirectParams.markersDrawCall = graphBuilder.AcquireExternalBufferView(m_markersData.drawCall->GetFullView());
	indirectParams.spheresDrawCall = graphBuilder.AcquireExternalBufferView(m_spheresData.drawCall->GetFullView());

	DebugGeometryPassConstants passConstants;
	passConstants.viewProjectionMatrix = settings.viewProjectionMatrix;
	passConstants.lines                = m_linesData.geometries->GetFullView();
	passConstants.markers              = m_markersData.geometries->GetFullView();
	passConstants.spheres              = m_spheresData.geometries->GetFullView();
	passConstants.sphereVertices       = m_sphereVerticesBuffer->GetFullView();

	lib::MTHandle<DebugGeometryPassDS> ds = graphBuilder.CreateDescriptorSet<DebugGeometryPassDS>(RENDERER_RESOURCE_NAME("Debug Geometry Pass DS"));
	ds->u_passConstants = passConstants;

	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Render Debug Geometry: {}", m_name.GetData()),
							renderPassDef,
							rg::BindDescriptorSets(std::move(ds)),
							std::tie(indirectParams),
							[resolution, indirectParams](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								const rdr::BufferView& linesDrawCall   = indirectParams.linesDrawCall->GetResourceRef();
								const rdr::BufferView& markersDrawCall = indirectParams.markersDrawCall->GetResourceRef();
								const rdr::BufferView& spheresDrawCall = indirectParams.spheresDrawCall->GetResourceRef();

								recorder.BindGraphicsPipeline(DebugGeometryPSO::lines);
								recorder.DrawIndirect(linesDrawCall, 0u, sizeof(rdr::HLSLStorage<DebugDrawCallData>), 1u);

								recorder.BindGraphicsPipeline(DebugGeometryPSO::markers);
								recorder.DrawIndirect(markersDrawCall, 0u, sizeof(rdr::HLSLStorage<DebugDrawCallData>), 1u);

								recorder.BindGraphicsPipeline(DebugGeometryPSO::spheres);
								recorder.DrawIndirect(spheresDrawCall, 0u, sizeof(rdr::HLSLStorage<DebugDrawCallData>), 1u);
							});

}

} // spt::gfx
