#include "ImGuiApplication.h"
#include "Bindless/BindlessTypes.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "Types/UIBackend.h"
#include "ImGui/SculptorImGui.h"
#include "Utils/TransfersManager.h"


namespace spt::closure_viewer
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

} // namespace utils


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


SIMPLE_COMPUTE_PSO(ClosureFurnanceTestPSO, "Sculptor/Tools/ClosureViewer/ClosureFurnanceTest.hlsl", ClosureFurnanceTestCS);


BEGIN_SHADER_STRUCT(ClosureFurnanceTestConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT();


void RenderFurnanceTest(rg::RenderGraphBuilder& graphBuilder, const lib::SharedPtr<rdr::TextureView>& output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	ClosureFurnanceTestConstants shaderConstants;
	shaderConstants.rwOutput = graphBuilder.AcquireExternalTextureView(output);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Furnance Test"),
						  ClosureFurnanceTestPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}


namespace closure_3d
{

constexpr rhi::EFragmentFormat colorFormat = rhi::EFragmentFormat::RGBA8_sRGB_Float;
constexpr rhi::EFragmentFormat depthFormat = rhi::EFragmentFormat::D32_S_Float;

constexpr Uint32 groundGridVerticesNum     = 6u;
constexpr Uint32 directionLinesVerticesNum = 6u;


rhi::GraphicsPipelineDefinition CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology topology)
{
	rhi::GraphicsPipelineDefinition pipelineDefinition;
	pipelineDefinition.primitiveTopology                       = topology;
	pipelineDefinition.rasterizationDefinition.cullMode        = rhi::ECullMode::None;
	pipelineDefinition.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition{ .format = colorFormat });
	pipelineDefinition.renderTargetsDefinition.depthRTDefinition.format = depthFormat;
	return pipelineDefinition;
}

} // namespace closure_3d


GRAPHICS_PSO(ClosureGroundGridPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   GroundGridVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", GroundGridFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, closure_3d::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::TriangleList), {});
	}
};


GRAPHICS_PSO(ClosureDirectionLinesPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   DirectionLinesVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", DirectionLinesFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, closure_3d::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::LineList), {});
	}
};


GRAPHICS_PSO(Closure3DPSO)
{
	VERTEX_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl",   Closure3DVS);
	FRAGMENT_SHADER("Sculptor/Tools/ClosureViewer/Closure3D.hlsl", Closure3DFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition pipelineDefinition = closure_3d::CreateOpaquePipelineDefinition(rhi::EPrimitiveTopology::TriangleList);

		rhi::ColorRenderTargetDefinition& colorRTDef = pipelineDefinition.renderTargetsDefinition.colorRTsDefinition[0];
		colorRTDef.colorBlendType = rhi::ERenderTargetBlendType::AlphaBlend;
		colorRTDef.alphaBlendType = rhi::ERenderTargetBlendType::AlphaBlend;
		pipelineDefinition.renderTargetsDefinition.depthRTDefinition.enableDepthWrite = false;

		pso = CompilePSO(compiler, pipelineDefinition, {});
	}
};


void RenderClosure3D(rg::RenderGraphBuilder& graphBuilder, const lib::SharedPtr<rdr::TextureView>& output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = output->GetResolution2D();

	const rg::RGTextureViewHandle color = graphBuilder.AcquireExternalTextureView(output);

	const rg::RGTextureViewHandle depth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Closure 3D Depth"), rg::TextureDef(resolution, closure_3d::depthFormat));

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
								recorder.DrawInstances(closure_3d::groundGridVerticesNum, 1u);

								recorder.BindGraphicsPipeline(ClosureDirectionLinesPSO::pso);
								recorder.DrawInstances(closure_3d::directionLinesVerticesNum, 1u);

								recorder.BindGraphicsPipeline(Closure3DPSO::pso);
								recorder.DrawInstances(closureVerticesNum, 1u);
							});
}


SIMPLE_COMPUTE_PSO(ClosureGraphPSO, "Sculptor/Tools/ClosureViewer/ClosureGraph.hlsl", ClosureGraphCS);


BEGIN_SHADER_STRUCT(ClosureGraphConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT();


void RenderClosureGraph(rg::RenderGraphBuilder& graphBuilder, const lib::SharedPtr<rdr::TextureView>& output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	ClosureGraphConstants shaderConstants;
	shaderConstants.rwOutput = graphBuilder.AcquireExternalTextureView(output);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Closure Graph"),
						  ClosureGraphPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}


SIMPLE_COMPUTE_PSO(Closure2DPSO, "Sculptor/Tools/ClosureViewer/Closure2D.hlsl", Closure2DCS);


BEGIN_SHADER_STRUCT(Closure2DConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwOutput)
END_SHADER_STRUCT();


void RenderClosure2D(rg::RenderGraphBuilder& graphBuilder, const lib::SharedPtr<rdr::TextureView>& output, const ClosureViewerData& viewerData)
{
	SPT_PROFILER_FUNCTION();

	Closure2DConstants shaderConstants;
	shaderConstants.rwOutput = graphBuilder.AcquireExternalTextureView(output);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Closure Graph"),
						  Closure2DPSO::pso,
						  math::Utils::DivideCeil(output->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants, viewerData));
}


class ClosureViewer : public tools::ImGuiApplication
{
protected:

	using Super = tools::ImGuiApplication;

public:

	ClosureViewer();

protected:

	// Begin tools::ImGuiApplication overrides
	virtual void DrawUI() override;
	virtual void RenderFrame(rg::RenderGraphBuilder& graphBuilder) override;
	// End tools::ImGuiApplication overrides

private:

	Bool DrawMaterialUI();
	void DrawClosure3DUI();

	void DrawFurnanceTestPanel();
	void DrawClosure3DPanel();
	void DrawClosure2DPanel();
	void DrawClosureGraphPanel();

	lib::SharedPtr<rdr::TextureView> CreateStorageTexture(const math::Vector2u& resolution, rhi::EFragmentFormat format) const;
	lib::SharedPtr<rdr::TextureView> CreateClosureRTTexture(const math::Vector2u& resolution) const;

	void UpdateView();

	lib::SharedPtr<rdr::TextureView> m_furnanceTestTexture;
	lib::SharedPtr<rdr::TextureView> m_closure3DTexture;
	lib::SharedPtr<rdr::TextureView> m_closure2DTexture;
	lib::SharedPtr<rdr::TextureView> m_closureGraphTexture;

	Real32 m_viewElevation = pi<Real32> * 0.25f;

	Real32 m_view3DPhi      = -pi<Real32> * 0.5f;
	Real32 m_view3DTheta    = pi<Real32> * 0.1f;
	Real32 m_view3DDistance = 3.f;
	Bool   m_view3DDirty    = true;

	ClosureViewerData m_viewerData;
};


ClosureViewer::ClosureViewer()
	: Super("ClosureViewer")
{
	const Uint32 latitudeSegments  = 200u;
	const Uint32 longitudeSegments = 200u;
	const lib::DynamicArray<math::Vector3f> closure3DVertices = utils::GenerateDebugSphereVertices(latitudeSegments, longitudeSegments);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = closure3DVertices.size() * sizeof(math::Vector3f);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	lib::SharedRef<rdr::Buffer> closure3DVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Closure 3D Vertices"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	rdr::GPUApi::GetTransfersManager().EnqueueUpload(closure3DVerticesBuffer, 0u, reinterpret_cast<const Byte*>(closure3DVertices.data()), bufferDef.size);

	m_viewerData.material.baseColor          = math::Vector3f::Constant(1.f);
	m_viewerData.material.metallic           = 0.f;
	m_viewerData.material.roughness          = 0.5f;
	m_viewerData.material.clearCoatWeight    = 0.f;
	m_viewerData.material.clearCoatRoughness = 0.1f;

	m_viewerData.closureScale   = 1.f;
	m_viewerData.closureOpacity = 0.5f;

	m_viewerData.frameIdx             = 0u;
	m_viewerData.closure3DVertices    = closure3DVerticesBuffer->GetFullView();
	m_viewerData.closure3DVerticesNum = static_cast<Uint32>(closure3DVertices.size());

	UpdateView();
}

void ClosureViewer::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::Begin("Settings");

	if (ImGui::Button("Capture Render Graph"))
	{
		RequestRenderGraphCapture();
	}

	if (ImGui::Button("HotReload Shaders"))
	{
		rdr::GPUApi::HotReloadShaders();
	}

	if (DrawMaterialUI())
	{
		m_viewerData.frameIdx = 0u;
	}

	DrawClosure3DUI();

	ImGui::End();

	ImGui::Begin("Closure");

	const Real32 panelWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

	ImGui::BeginChild("Left Column", ImVec2(panelWidth, 0.f), false);
	{
		const Real32 panelHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

		ImGui::BeginChild("Furnance Test Panel", ImVec2(0.f, panelHeight), true);
		DrawFurnanceTestPanel();
		ImGui::EndChild();

		ImGui::BeginChild("Closure 2D", ImVec2(0.f, 0.f), true);
		DrawClosure2DPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Right Column", ImVec2(0.f, 0.f), false);
	{
		const Real32 panelHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

		ImGui::BeginChild("Closure 3D", ImVec2(0.f, panelHeight), true);
		DrawClosure3DPanel();
		ImGui::EndChild();

		ImGui::BeginChild("Closure Graph Panel", ImVec2(0.f, 0.f), true);
		DrawClosureGraphPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::End();
}

namespace ui_utils
{

math::Vector2u GetAvailableTextureResolution()
{
	const ImVec2 available = ImGui::GetContentRegionAvail();
	return math::Vector2u(static_cast<Uint32>(std::max(available.x, 16.f)), static_cast<Uint32>(std::max(available.y, 16.f)));
}

} // namespace ui_utils

void ClosureViewer::DrawFurnanceTestPanel()
{
	ImGui::TextUnformatted("Furnance Test");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_furnanceTestTexture || resolution != m_furnanceTestTexture->GetResolution2D())
	{
		m_furnanceTestTexture = CreateStorageTexture(resolution, rhi::EFragmentFormat::RGBA32_S_Float);
		m_viewerData.frameIdx = 0u;
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_furnanceTestTexture)), imageSize);
}

void ClosureViewer::DrawClosure3DPanel()
{
	ImGui::TextUnformatted("Closure 3D");

	if (ImGui::IsKeyDown(ImGuiKey_MouseLeft) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
	{
		const ImVec2 mousDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.f);
		ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

		m_view3DPhi   += mousDrag.x * 0.005f;
		m_view3DTheta += mousDrag.y * 0.005f;

		m_view3DDirty = true;
	}

	const Real32 mouseWheel = ImGui::GetIO().MouseWheel;
	if (mouseWheel != 0.f)
	{
		m_view3DDistance *= std::pow(0.9f, mouseWheel);
		m_view3DDirty     = true;
	}

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closure3DTexture || resolution != m_closure3DTexture->GetResolution2D())
	{
		m_closure3DTexture = CreateClosureRTTexture(resolution);
		UpdateView();
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closure3DTexture)), imageSize);
}

void ClosureViewer::DrawClosure2DPanel()
{
	ImGui::TextUnformatted("Closure 2D");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closure2DTexture || resolution != m_closure2DTexture->GetResolution2D())
	{
		m_closure2DTexture = CreateStorageTexture(resolution, rhi::EFragmentFormat::RGBA8_UN_Float);
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closure2DTexture)), imageSize);
}

void ClosureViewer::DrawClosureGraphPanel()
{
	ImGui::TextUnformatted("Closure Graph");

	const math::Vector2u resolution = ui_utils::GetAvailableTextureResolution();
	if (!m_closureGraphTexture || resolution != m_closureGraphTexture->GetResolution2D())
	{
		m_closureGraphTexture = CreateStorageTexture(resolution, rhi::EFragmentFormat::RGBA8_UN_Float);
	}

	const math::Vector2f imageSize = resolution.cast<Real32>();
	ImGui::Image(rdr::UIBackend::GetUITextureID(lib::Ref(m_closureGraphTexture)), imageSize);
}

lib::SharedPtr<rdr::TextureView> ClosureViewer::CreateStorageTexture(const math::Vector2u& resolution, rhi::EFragmentFormat format) const
{
	rhi::TextureDefinition textureDef;
	textureDef.resolution = resolution;
	textureDef.format     = format;
	textureDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
	return rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Closure Storage Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);
}

lib::SharedPtr<rdr::TextureView> ClosureViewer::CreateClosureRTTexture(const math::Vector2u& resolution) const
{
	// sRGB formats can't be used as storage textures
	rhi::TextureDefinition textureDef;
	textureDef.resolution = resolution;
	textureDef.format     = closure_3d::colorFormat;
	textureDef.usage      = lib::Flags(rhi::ETextureUsage::ColorRT, rhi::ETextureUsage::SampledTexture);
	return rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Closure Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);
}

void ClosureViewer::RenderFrame(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	RenderFurnanceTest(graphBuilder, m_furnanceTestTexture, m_viewerData);

	RenderClosure3D(graphBuilder, m_closure3DTexture, m_viewerData);

	RenderClosure2D(graphBuilder, m_closure2DTexture, m_viewerData);

	RenderClosureGraph(graphBuilder, m_closureGraphTexture, m_viewerData);

	++m_viewerData.frameIdx;
}

Bool ClosureViewer::DrawMaterialUI()
{
	ClosureMaterial& material = m_viewerData.material;

	Bool changed = false;

	ImGui::Separator();
	ImGui::TextUnformatted("Material");
	changed |= ImGui::ColorEdit3("Base Color", material.baseColor.data());
	changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Clear Coat Weight", &material.clearCoatWeight, 0.f, 1.f);
	changed |= ImGui::SliderFloat("Clear Coat Roughness", &material.clearCoatRoughness, 0.f, 1.f);

	return changed;
}

void ClosureViewer::DrawClosure3DUI()
{
	ImGui::Separator();
	ImGui::TextUnformatted("Closure 3D");

	if (ImGui::SliderAngle("View Elevation", &m_viewElevation, 0.f, 90.f) || m_view3DDirty)
	{
		UpdateView();
		m_view3DDirty = false;
	}

	ImGui::SliderFloat("Closure Scale", &m_viewerData.closureScale, 0.01f, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Closure Opacity", &m_viewerData.closureOpacity, 0.f, 1.f);

	ImGui::TextColored(ImVec4(1.f, 0.85f, 0.1f, 1.f), "View");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Mirror");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.f, 1.f), "Normal");
}

void ClosureViewer::UpdateView()
{
	m_viewerData.viewDirection = math::Vector3f(std::cos(m_viewElevation), 0.f, std::sin(m_viewElevation));

//	const math::Vector3f cameraLocation = math::Vector3f(0.f, -3.5f, 1.5f);

	const math::Vector3f cameraLocation = math::Vector3f(m_view3DDistance * std::cos(m_view3DPhi) * std::cos(m_view3DTheta),
														 m_view3DDistance * std::sin(m_view3DPhi) * std::cos(m_view3DTheta),
														 m_view3DDistance * std::sin(m_view3DTheta));

	const math::Vector3f cameraTarget   = math::Vector3f(0.f, 0.f, 0.4f);

	const math::Vector2f resolution = m_closure3DTexture ? m_closure3DTexture->GetResolution2D().cast<Real32>() : math::Vector2f(1.f, 1.f);

	const math::Matrix4f projectionMatrix = utils::CreatePerspectiveProjection(pi<Real32> * 0.25f, resolution, 0.01f);
	const math::Matrix4f viewMatrix       = utils::CreateLookAtMatrix(cameraLocation, cameraTarget);

	m_viewerData.viewProjectionMatrix = projectionMatrix * viewMatrix;
}

} // spt::closure_viewer


int main()
{
	return spt::tools::RunApplication<spt::closure_viewer::ClosureViewer>();
}
