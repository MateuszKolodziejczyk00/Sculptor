#include "ReferenceTool.h"
#include "Engine.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "Utils/TransfersManager.h"
#include "RenderGraphUtils.h"
#include "Loaders/TextureLoader.h"
#include "Serialization.h"
#include "SerializationHelper.h"

#include <cstdlib>


namespace spt::closure_viewer::ref_tool
{

inline const math::Vector2u captureResolution(1024u, 1024u);

constexpr Uint32 furnanceTestSamplesNum = 1024u;

constexpr const char* materialFileName     = "Material.json";
constexpr const char* furnanceTestFileName = "FurnanceTest.png";
constexpr const char* closure3DFileName    = "Closure3D.png";
constexpr const char* closure2DFileName    = "Closure2D.png";
constexpr const char* closureGraphFileName = "ClosureGraph.png";

constexpr const char* newFilePrefix  = "New_";
constexpr const char* flipFilePrefix = "Flip_";
constexpr const char* flipStatsFileName = "FlipStats.txt";

constexpr const char* flipScriptRelativePath = "Source/Tools/ClosureViewer/GenerateFlipImages.py";

constexpr Real32 closureScale   = 1.f;
constexpr Real32 closureOpacity = 0.5f;


struct MaterialRecord
{
	math::Vector3f baseColor          = math::Vector3f::Constant(1.f);
	Real32         metallic           = 0.f;
	Real32         roughness          = 0.5f;
	Real32         clearCoatWeight    = 0.f;
	Real32         clearCoatRoughness = 0.1f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("baseColor", baseColor);
		serializer.Serialize("metallic", metallic);
		serializer.Serialize("roughness", roughness);
		serializer.Serialize("clearCoatWeight", clearCoatWeight);
		serializer.Serialize("clearCoatRoughness", clearCoatRoughness);
	}
};


struct RaportImage
{
	const char* title;
	const char* elementId;
	const char* fileName;
};


constexpr RaportImage raportImages[] =
{
	{ "Furnance Test", "furnanceTest", furnanceTestFileName },
	{ "Closure 3D",    "closure3D",    closure3DFileName },
	{ "Closure 2D",    "closure2D",    closure2DFileName },
	{ "Closure Graph", "closureGraph", closureGraphFileName },
};


namespace html_builder
{

lib::String FormatColor(const math::Vector3f& color)
{
	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%.3f, %.3f, %.3f", color.x(), color.y(), color.z());
	return lib::String(buffer);
}


lib::String FormatReal6(Real32 value)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%.6f", value);
	return lib::String(buffer);
}


lib::String FormatReal(Real32 value)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%.3f", value);
	return lib::String(buffer);
}


void AppendMaterialRow(lib::String& html, const char* propertyName, const lib::String& referenceValue, const lib::String& newValue, Bool isDifferent)
{
	html += "<tr class=\"";
	html += (isDifferent ? "diff-row" : "same-row");
	html += "\"><td>";
	html += propertyName;
	html += "</td><td>";
	html += referenceValue;
	html += "</td><td>";
	html += newValue;
	html += "</td></tr>\n";
}


lib::String BuildMaterialComparisonTable(const MaterialRecord& reference, const MaterialRecord& newMaterial)
{
	constexpr Real32 epsilon = 0.0005f;

	lib::String html = "<table class=\"material-table\">\n<tr><th>Property</th><th>Reference</th><th>New</th></tr>\n";

	AppendMaterialRow(html, "Base Color",            FormatColor(reference.baseColor),         FormatColor(newMaterial.baseColor),         !reference.baseColor.isApprox(newMaterial.baseColor, epsilon));
	AppendMaterialRow(html, "Metallic",              FormatReal(reference.metallic),           FormatReal(newMaterial.metallic),           std::abs(reference.metallic - newMaterial.metallic) > epsilon);
	AppendMaterialRow(html, "Roughness",             FormatReal(reference.roughness),          FormatReal(newMaterial.roughness),          std::abs(reference.roughness - newMaterial.roughness) > epsilon);
	AppendMaterialRow(html, "Clear Coat Weight",     FormatReal(reference.clearCoatWeight),    FormatReal(newMaterial.clearCoatWeight),    std::abs(reference.clearCoatWeight - newMaterial.clearCoatWeight) > epsilon);
	AppendMaterialRow(html, "Clear Coat Roughness",  FormatReal(reference.clearCoatRoughness), FormatReal(newMaterial.clearCoatRoughness), std::abs(reference.clearCoatRoughness - newMaterial.clearCoatRoughness) > epsilon);

	html += "</table>\n";

	return html;
}


void AppendImageCompareSlider(lib::String& html, const char* title, const char* elementId, const lib::String& referenceImagePath, const lib::String& newImagePath)
{
	html += "<div class=\"compare-block\">\n";
	html += "<h3>"; html += title; html += " - Reference / New</h3>\n";
	html += "<div class=\"compare-container\" id=\""; html += elementId; html += "\">\n";
	html += "<img class=\"compare-new\" src=\""; html += newImagePath; html += "\" draggable=\"false\">\n";
	html += "<div class=\"compare-overlay\"><img class=\"compare-reference\" src=\""; html += referenceImagePath; html += "\" draggable=\"false\"></div>\n";
	html += "<div class=\"compare-divider\"></div>\n";
	html += "<input type=\"range\" min=\"0\" max=\"100\" value=\"50\" class=\"compare-slider\" oninput=\"UpdateCompareSlider('"; html += elementId; html += "', this.value)\">\n";
	html += "<div class=\"compare-label compare-label-left\">Reference</div>\n";
	html += "<div class=\"compare-label compare-label-right\">New</div>\n";
	html += "</div>\n</div>\n";
}


struct FlipStats
{
	Real32 mean   = 0.f;
	Real32 median = 0.f;
	Real32 p95    = 0.f;
	Real32 p99    = 0.f;
	Real32 max    = 0.f;
};


lib::HashMap<lib::String, FlipStats> LoadFlipStats(const lib::Path& statsPath)
{
	lib::HashMap<lib::String, FlipStats> stats;

	std::ifstream file(statsPath.generic_string());
	std::string name;
	FlipStats entry;
	while (file >> name >> entry.mean >> entry.median >> entry.p95 >> entry.p99 >> entry.max)
	{
		stats[lib::String(name)] = entry;
	}

	return stats;
}


void AppendFlipImage(lib::String& html, const lib::String& flipImagePath, const FlipStats* stats)
{
	html += "<div class=\"compare-block\">\n<h3>FLIP error map</h3>\n";
	html += "<img class=\"flip-image\" src=\""; html += flipImagePath; html += "\">\n";
	if (stats)
	{
		html += "<table class=\"material-table\">\n";
		html += "<tr><th>Mean</th><th>Median</th><th>95th pct</th><th>99th pct</th><th>Max</th></tr>\n";
		html += "<tr><td>" + FormatReal6(stats->mean) + "</td><td>" + FormatReal6(stats->median) + "</td><td>" + FormatReal6(stats->p95)
			+ "</td><td>" + FormatReal6(stats->p99) + "</td><td>" + FormatReal6(stats->max) + "</td></tr>\n";
		html += "</table>\n";
	}
	html += "</div>\n";
}


lib::String ToRaportRelativePath(const lib::Path& path, const lib::Path& raportDirectory)
{
	std::error_code errorCode;
	const lib::Path relativePath = std::filesystem::relative(path, raportDirectory, errorCode);
	return (errorCode || relativePath.empty()) ? std::filesystem::absolute(path).generic_string() : relativePath.generic_string();
}


lib::String BuildComparisonRaportHtml(const MaterialRecord& reference, const MaterialRecord& newMaterial, const lib::Path& referenceDirectory, const lib::Path& raportDirectory)
{
	lib::String html;
	html.reserve(8192);

	html += "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>Closure Viewer Comparison Raport</title>\n";
	html += R"HTMLSTYLE(<style>
body { font-family: Segoe UI, Arial, sans-serif; background: #1e1e1e; color: #ddd; margin: 24px; }
h1 { color: #fff; }
h3 { color: #ccc; margin-bottom: 8px; }
.column { display: flex; flex-direction: column; gap: 24px; }
.row { display: flex; flex-direction: row; gap: 24px; align-items: flex-start; }
.flip-image { width: 1024px; height: 1024px; display: block; border-radius: 4px; }
.compare-block { background: #2a2a2a; padding: 12px; border-radius: 8px; }
.compare-container { position: relative; width: 1024px; height: 1024px; overflow: hidden; border-radius: 4px; user-select: none; }
.compare-container img { position: absolute; top: 0; left: 0; width: 1024px; height: 1024px; display: block; pointer-events: none; }
.compare-overlay { position: absolute; top: 0; left: 0; width: 50%; height: 100%; overflow: hidden; }
.compare-divider { position: absolute; top: 0; left: 50%; width: 2px; height: 100%; background: #ffd21a; pointer-events: none; }
.compare-slider { position: absolute; left: 0; top: 0; width: 100%; height: 100%; margin: 0; opacity: 0; cursor: ew-resize; }
.compare-label { position: absolute; top: 8px; background: rgba(0,0,0,0.55); color: #fff; padding: 2px 8px; border-radius: 4px; font-size: 12px; pointer-events: none; }
.compare-label-left { left: 8px; }
.compare-label-right { right: 8px; }
.material-table { border-collapse: collapse; margin-top: 12px; }
.material-table th, .material-table td { border: 1px solid #444; padding: 6px 12px; text-align: left; }
.material-table th { background: #333; }
.diff-row { background: #4a2f1a; color: #ffcf8f; }
.same-row { background: #262626; }
</style>
)HTMLSTYLE";
	html += "</head>\n<body>\n";
	html += "<h1>Closure Viewer Comparison Raport</h1>\n";

	html += "<h2>Material</h2>\n";
	html += BuildMaterialComparisonTable(reference, newMaterial);

	const lib::HashMap<lib::String, FlipStats> flipStats = LoadFlipStats(raportDirectory / flipStatsFileName);

	html += "<h2>Images</h2>\n<div class=\"column\">\n";

	for (const RaportImage& image : raportImages)
	{
		html += "<div class=\"row\">\n";
		AppendImageCompareSlider(html, image.title, image.elementId, ToRaportRelativePath(referenceDirectory / image.fileName, raportDirectory), lib::String(newFilePrefix) + image.fileName);

		const lib::String flipFileName = lib::String(flipFilePrefix) + image.fileName;
		if (std::filesystem::exists(raportDirectory / flipFileName))
		{
			const auto statsIt = flipStats.find(image.fileName);
			AppendFlipImage(html, flipFileName, statsIt != flipStats.end() ? &statsIt->second : nullptr);
		}
		html += "</div>\n";
	}

	html += "</div>\n";
	html += R"HTMLSCRIPT(<script>
function UpdateCompareSlider(id, value)
{
	const container = document.getElementById(id);
	const overlay = container.querySelector('.compare-overlay');
	const divider = container.querySelector('.compare-divider');
	overlay.style.width = value + '%';
	divider.style.left = value + '%';
}
</script>
)HTMLSCRIPT";

	html += "</body>\n</html>\n";

	return html;
}

} // html_builder


ClosureViewerData BuildViewerData(const ClosureMaterial& material, lib::SharedPtr<rdr::Buffer>& outClosure3DVerticesBuffer)
{
	constexpr Uint32 latitudeSegments  = 200u;
	constexpr Uint32 longitudeSegments = 200u;
	const lib::DynamicArray<math::Vector3f> closure3DVertices = renderer::utils::GenerateDebugSphereVertices(latitudeSegments, longitudeSegments);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = closure3DVertices.size() * sizeof(math::Vector3f);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	const lib::SharedRef<rdr::Buffer> closure3DVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Reference Tool Closure 3D Vertices"), bufferDef, rhi::EMemoryUsage::GPUOnly);
	outClosure3DVerticesBuffer = closure3DVerticesBuffer.ToSharedPtr();

	rdr::GPUApi::GetTransfersManager().EnqueueUpload(closure3DVerticesBuffer, 0u, reinterpret_cast<const Byte*>(closure3DVertices.data()), bufferDef.size);

	ClosureViewerData viewerData;
	viewerData.material                = material;
	viewerData.closureScale            = closureScale;
	viewerData.closureOpacity          = closureOpacity;
	viewerData.frameIdx                = 0u;
	viewerData.closure3DVertices       = closure3DVerticesBuffer->GetFullView();
	viewerData.closure3DVerticesNum    = static_cast<Uint32>(closure3DVertices.size());

	constexpr Real32 viewElevation = pi<Real32> * 0.25f;
	viewerData.viewDirection = math::Vector3f(std::cos(viewElevation), 0.f, std::sin(viewElevation));

	constexpr Real32 phi      = -pi<Real32> * 0.5f;
	constexpr Real32 theta    = pi<Real32> * 0.1f;
	constexpr Real32 distance = 3.f;
	const math::Vector3f cameraLocation = math::Vector3f(distance * std::cos(phi) * std::cos(theta),
														 distance * std::sin(phi) * std::cos(theta),
														 distance * std::sin(theta));
	const math::Vector3f cameraTarget(0.f, 0.f, 0.4f);

	const math::Matrix4f projectionMatrix = renderer::utils::CreatePerspectiveProjection(pi<Real32> * 0.25f, captureResolution.cast<Real32>(), 0.01f);
	const math::Matrix4f viewMatrix       = renderer::utils::CreateLookAtMatrix(cameraLocation, cameraTarget);

	viewerData.viewProjectionMatrix = projectionMatrix * viewMatrix;

	return viewerData;
}


Bool RenderAndSaveImages(const ClosureMaterial& material, const lib::Path& outputDirectory, const lib::String& filePrefix)
{
	SPT_PROFILER_FUNCTION();

	lib::SharedPtr<rdr::Buffer> closure3DVerticesBuffer;
	const ClosureViewerData viewerData = BuildViewerData(material, closure3DVerticesBuffer);

	js::JobWithResult<Bool> furnanceTestSaveJob;
	js::JobWithResult<Bool> closure3DSaveJob;
	js::JobWithResult<Bool> closure2DSaveJob;
	js::JobWithResult<Bool> closureGraphSaveJob;

	tools::ExecuteRenderGraph([&](rg::RenderGraphBuilder& graphBuilder)
	{
		const rg::RGTextureViewHandle furnanceTestView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reference Furnance Test"), rg::TextureDef(captureResolution, rhi::EFragmentFormat::RGBA32_S_Float));

		renderer::RenderFurnanceTest(graphBuilder, furnanceTestView, viewerData, furnanceTestSamplesNum);

		const rg::RGTextureViewHandle furnanceTestLDR = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Furnance Test LDR"), rg::TextureDef(furnanceTestView->GetResolution2D(), rhi::EFragmentFormat::RGBA8_UN_Float));

		graphBuilder.BlitTexture(RG_DEBUG_NAME("Furnance Test Blit To LDR"), furnanceTestView, furnanceTestLDR, rhi::ESamplerFilterType::Nearest);

		const rg::RGTextureViewHandle closure3DView    = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reference Closure 3D"), rg::TextureDef(captureResolution, renderer::closure3DColorFormat));
		const rg::RGTextureViewHandle closure2DView    = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reference Closure 2D"), rg::TextureDef(captureResolution, rhi::EFragmentFormat::RGBA8_UN_Float));
		const rg::RGTextureViewHandle closureGraphView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reference Closure Graph"), rg::TextureDef(captureResolution, rhi::EFragmentFormat::RGBA8_UN_Float));

		renderer::RenderClosure3D(graphBuilder, closure3DView, viewerData);
		renderer::RenderClosure2D(graphBuilder, closure2DView, viewerData);
		renderer::RenderClosureGraph(graphBuilder, closureGraphView, viewerData);

		furnanceTestSaveJob = gfx::TextureWriter::SaveTexture(graphBuilder, furnanceTestLDR, (outputDirectory / (filePrefix + furnanceTestFileName)).generic_string());
		closure3DSaveJob    = gfx::TextureWriter::SaveTexture(graphBuilder, closure3DView, (outputDirectory / (filePrefix + closure3DFileName)).generic_string());
		closure2DSaveJob    = gfx::TextureWriter::SaveTexture(graphBuilder, closure2DView, (outputDirectory / (filePrefix + closure2DFileName)).generic_string());
		closureGraphSaveJob = gfx::TextureWriter::SaveTexture(graphBuilder, closureGraphView, (outputDirectory / (filePrefix + closureGraphFileName)).generic_string());
	});

	const Bool furnanceTestSaved = furnanceTestSaveJob.Await();
	const Bool closure3DSaved    = closure3DSaveJob.Await();
	const Bool closure2DSaved    = closure2DSaveJob.Await();
	const Bool closureGraphSaved = closureGraphSaveJob.Await();

	return furnanceTestSaved && closure3DSaved && closure2DSaved && closureGraphSaved;
}


Bool GenerateFlipImages(const lib::Path& referenceDirectory, const lib::Path& raportDirectory)
{
	const lib::Path scriptPath = std::filesystem::absolute(engn::Engine::Get().GetPaths().enginePath / flipScriptRelativePath);
	SPT_CHECK_MSG(std::filesystem::exists(scriptPath), "FLIP script not found at {}", scriptPath.generic_string());

	// Outer quotes are required by cmd.exe when the command itself starts with a quoted token
	lib::String command = "python \"" + scriptPath.string() + "\" \"" + std::filesystem::absolute(referenceDirectory).string() + "\" \"" + std::filesystem::absolute(raportDirectory).string() + "\"";
	for (const RaportImage& image : raportImages)
	{
		command += " ";
		command += image.fileName;
	}
#if SPT_PLATFORM_WINDOWS
	command = "\"" + command + "\"";
#endif

	const Int32 result = std::system(command.c_str());
	if (result != 0)
	{
		SPT_LOG_WARN(ClosureViewer, "FLIP image generation failed (exit code {}). Make sure python and 'pip install flip-evaluator pillow numpy' are available. Raport will not contain FLIP images", result);
		return false;
	}

	return true;
}

Bool CreateReference(const ClosureMaterial& material, const lib::Path& outputDirectory)
{
	SPT_PROFILER_FUNCTION();

	std::filesystem::create_directories(outputDirectory);

	MaterialRecord record;
	record.baseColor          = material.baseColor;
	record.metallic           = material.metallic;
	record.roughness          = material.roughness;
	record.clearCoatWeight    = material.clearCoatWeight;
	record.clearCoatRoughness = material.clearCoatRoughness;

	srl::SerializationHelper::SaveTextStructToFile(record, (outputDirectory / materialFileName).generic_string());

	const Bool success = RenderAndSaveImages(material, outputDirectory, lib::String());

	if (success)
	{
		SPT_LOG_INFO(ClosureViewer, "Reference created at {}", outputDirectory.generic_string());
	}
	else
	{
		SPT_LOG_ERROR(ClosureViewer, "Failed to render/save reference images at {}", outputDirectory.generic_string());
	}

	return success;
}


Bool GenerateComparisonRaport(const lib::Path& referenceDirectory, const lib::Path& raportPath)
{
	SPT_PROFILER_FUNCTION();

	const lib::Path materialPath = referenceDirectory / materialFileName;
	if (!std::filesystem::exists(materialPath))
	{
		SPT_LOG_ERROR(ClosureViewer, "Reference material not found at {}", materialPath.generic_string());
		return false;
	}

	MaterialRecord reference;
	if (!srl::SerializationHelper::LoadTextStructFromFile(reference, materialPath.generic_string()))
	{
		SPT_LOG_ERROR(ClosureViewer, "Failed to load reference material from {}", materialPath.generic_string());
		return false;
	}

	ClosureMaterial material;
	material.baseColor          = reference.baseColor;
	material.metallic           = reference.metallic;
	material.roughness          = reference.roughness;
	material.clearCoatWeight    = reference.clearCoatWeight;
	material.clearCoatRoughness = reference.clearCoatRoughness;

	lib::Path raportDirectory = raportPath.parent_path();
	if (raportDirectory.empty())
	{
		raportDirectory = ".";
	}
	std::filesystem::create_directories(raportDirectory);

	const Bool renderSuccess = RenderAndSaveImages(material, raportDirectory, lib::String(newFilePrefix));
	if (!renderSuccess)
	{
		SPT_LOG_ERROR(ClosureViewer, "Failed to render/save new images for comparison raport at {}", raportDirectory.generic_string());
		return false;
	}

	GenerateFlipImages(referenceDirectory, raportDirectory);

	const lib::String html = html_builder::BuildComparisonRaportHtml(reference, reference, referenceDirectory, raportDirectory);

	lib::File::SaveDocument(raportPath, html, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent));

	SPT_LOG_INFO(ClosureViewer, "Comparison raport generated at {}", raportPath.generic_string());

#if SPT_PLATFORM_WINDOWS
	const lib::String openCommand = "start \"\" \"" + std::filesystem::absolute(raportPath).string() + "\"";
	std::system(openCommand.c_str());
#endif

	return true;
}

} // spt::closure_viewer::ref_tool
