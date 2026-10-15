#include "Bindless/BindlessTypes.h"
#include "GlobalShaderResources/GlobalShaderResources.h"
#include "SculptorCoreTypes.h"
#include "Engine.h"
#include "JobSystem.h"
#include "ScopedEngine.h"
#include "RenderGraphUtils.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Loaders/TextureLoader.h"


SPT_DEFINE_LOG_CATEGORY(GGXMultiscatteringLUTsGenerator, true);


namespace spt::lut_gen
{

struct GenerationContext
{
	explicit GenerationContext(lib::Path inOutputDirectory)
		: outputDirectory(std::move(inOutputDirectory))
	{ }

	void SaveLUT(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle lut, const lib::String& fileName)
	{
		const lib::String path = (outputDirectory / fileName).generic_string();
		saveJobs.emplace_back(gfx::TextureWriter::SaveTexture(graphBuilder, lut, path));
	}

	lib::Path outputDirectory;

	lib::DynamicArray<js::JobWithResult<Bool>> saveJobs;
};


namespace roughness_view_lut
{

SIMPLE_COMPUTE_PSO(GenerateRoughnessViewLUTPSO, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateRoughnessViewLUT.hlsl", GenerateRoughnessViewLUTCS);


BEGIN_SHADER_STRUCT(GenerateRoughnessViewLUTConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, rwLUT)
	SHADER_STRUCT_FIELD(Uint32,                    seed)
END_SHADER_STRUCT()


void Generate(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u lutResolution(32u, 32u);

	SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "Generating roughness view LUT");

	const rg::RGTextureViewHandle lut = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX_E_LUT"), rg::TextureDef(lutResolution, rhi::EFragmentFormat::R32_S_Float));

	GenerateRoughnessViewLUTConsts shaderParams;
	shaderParams.rwLUT = lut;
	shaderParams.seed  = 235498234u;

	graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateRoughnessViewLUT"),
							GenerateRoughnessViewLUTPSO::pso,
							math::Utils::DivideCeil(lutResolution, math::Vector2u(8u, 8u)),
							rg::ShaderParams(shaderParams));

	context.SaveLUT(graphBuilder, lut, "GGX_E_LUT.dds");
}

} // roughness_view_lut

namespace e_avg_lut
{

SIMPLE_COMPUTE_PSO(GenerateEAvgLUTPSO, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateEAvgLUT.hlsl", GenerateEAvgLUTCS);


BEGIN_SHADER_STRUCT(GenerateEAvgLUTConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, rwLUT)
	SHADER_STRUCT_FIELD(Uint32,                    seed)
END_SHADER_STRUCT()


void Generate(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u lutResolution(32u, 1u);

	SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "Generating E_avg LUT");

	const rg::RGTextureViewHandle lut = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX_EAvg_LUT"), rg::TextureDef(lutResolution, rhi::EFragmentFormat::R32_S_Float).SetType(rhi::ETextureType::Texture2D));

	GenerateEAvgLUTConsts shaderParams;
	shaderParams.rwLUT = lut;
	shaderParams.seed  = 9182371u;

	graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateEAvgLUT"),
							GenerateEAvgLUTPSO::pso,
							math::Utils::DivideCeil(lutResolution, math::Vector2u(32u, 1u)),
							rg::ShaderParams(shaderParams));

	context.SaveLUT(graphBuilder, lut, "GGX_EAvg_LUT.dds");
}

} // e_avg_lut


namespace ggx_throughput_lut
{

SIMPLE_COMPUTE_PSO(GenerateGGXDirectionalAlbedoFresnelPSO, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateGGXDirectionalAlbedo_Fresnel.hlsl", GenerateGGXDirectionalAlbedoFresnelCS);


BEGIN_SHADER_STRUCT(GenerateGGXDirectionalAlbedoFresnelConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<Real32>, rwLUT)
	SHADER_STRUCT_FIELD(Uint32,                    seed)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(GenerateGGXAverageEnergy_Fresnel, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateGGXAverageEnergy_Fresnel.hlsl", GenerateGGXAverageEnergy_FresnelCS);


BEGIN_SHADER_STRUCT(GenerateGGXAverageEnergyFresnelConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,  rwLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>,  directionalAlbedoLUT)
	SHADER_STRUCT_FIELD(Uint32,                     seed)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(GenerateGGXThroughputLUTPSO, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateGGXThroughput_LUT.hlsl", GenerateGGXThroughput_LUTCS);


BEGIN_SHADER_STRUCT(GenerateGGXThroughputLUTConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<Real32>,  rwLUT)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<Real32>,  rwCoatLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>,  directionalAlbedoLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,  averageEnergyLUT)
	SHADER_STRUCT_FIELD(Uint32,                     seed)
END_SHADER_STRUCT()


void Generate(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "Generating GGX throughput LUT");

	const rg::RGTextureViewHandle averageAlbedo = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX Average Albedo Fresnel"), rg::TextureDef(math::Vector3u(64u, 64u, 64u), rhi::EFragmentFormat::R32_S_Float).SetType(rhi::ETextureType::Texture3D));

	{
		GenerateGGXDirectionalAlbedoFresnelConsts shaderParams;
		shaderParams.rwLUT = averageAlbedo;
		shaderParams.seed  = 123456789u;

		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateGGXDirectionalAlbedoFresnel"),
								GenerateGGXDirectionalAlbedoFresnelPSO::pso,
								math::Utils::DivideCeil(averageAlbedo->GetResolution(), math::Vector3u(4u, 4u, 4u)),
								rg::ShaderParams(shaderParams));
	}

	const rg::RGTextureViewHandle averageEnergy = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX Average Energy Fresnel"), rg::TextureDef(math::Vector2u(64u, 64u), rhi::EFragmentFormat::R32_S_Float));

	{
		GenerateGGXAverageEnergyFresnelConsts shaderParams;
		shaderParams.rwLUT                = averageEnergy;
		shaderParams.directionalAlbedoLUT = averageAlbedo;
		shaderParams.seed                 = 987654321u;

		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateGGXAverageEnergy_Fresnel"),
								GenerateGGXAverageEnergy_Fresnel::pso,
								math::Utils::DivideCeil(averageEnergy->GetResolution2D(), math::Vector2u(8u, 8u)),
								rg::ShaderParams(shaderParams));
	}

	const rg::RGTextureViewHandle throughputLUT     = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX Throughput LUT"), rg::TextureDef(math::Vector3u(16u, 16u, 16u), rhi::EFragmentFormat::R32_S_Float).SetType(rhi::ETextureType::Texture3D));
	const rg::RGTextureViewHandle coatThroughputLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX Coat Throughput LUT"), rg::TextureDef(math::Vector3u(16u, 16u, 16u), rhi::EFragmentFormat::R32_S_Float).SetType(rhi::ETextureType::Texture3D));

	{
		GenerateGGXThroughputLUTConsts shaderParams;
		shaderParams.rwLUT                = throughputLUT;
		shaderParams.rwCoatLUT            = coatThroughputLUT;
		shaderParams.directionalAlbedoLUT = averageAlbedo;
		shaderParams.averageEnergyLUT     = averageEnergy;
		shaderParams.seed                 = 192837465u;

		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateGGXThroughput_LUT"),
								GenerateGGXThroughputLUTPSO::pso,
								math::Utils::DivideCeil(throughputLUT->GetResolution(), math::Vector3u(4u, 4u, 4u)),
								rg::ShaderParams(shaderParams));
	}

	context.SaveLUT(graphBuilder, throughputLUT, "GGX_Throughput_LUT.dds");
}

} // ggx_throughput_lut


namespace coating_throughput_lut
{

SIMPLE_COMPUTE_PSO(GenerateCoatingThroughputLUTPSO, "Sculptor/Tools/GGXMultiscatteringLUTs/GenerateGGXCoatingThroughputLUT.hlsl", GenerateGGXCoatingThroughputLUTCS);


BEGIN_SHADER_STRUCT(GenerateGGXCoatingThroughputLUTConsts)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,  rwCoatLUT)
	SHADER_STRUCT_FIELD(Uint32,                     seed)
END_SHADER_STRUCT()


void Generate(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "Generating GGX coating throughput LUT");

	const rg::RGTextureViewHandle coatThroughputLUT = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GGX Coat Throughput LUT"), rg::TextureDef(math::Vector2u(32u, 32u), rhi::EFragmentFormat::R32_S_Float));

	GenerateGGXCoatingThroughputLUTConsts shaderParams;
	shaderParams.rwCoatLUT = coatThroughputLUT;
	shaderParams.seed      = 564738291u;

	graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateGGXCoatingThroughputLUT"),
							GenerateCoatingThroughputLUTPSO::pso,
							math::Utils::DivideCeil(coatThroughputLUT->GetResolution2D(), math::Vector2u(8u, 8u)),
							rg::ShaderParams(shaderParams));

	context.SaveLUT(graphBuilder, coatThroughputLUT, "GGX_Coat_Throughput_LUT.dds");
}

} // coating_throughput_lut

static void GenerateMultiscatteringLUTs(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	roughness_view_lut::Generate(graphBuilder, context);

	e_avg_lut::Generate(graphBuilder, context);
}


static void GenerateThroughputLUT(rg::RenderGraphBuilder& graphBuilder, GenerationContext& context)
{
	SPT_PROFILER_FUNCTION();

	ggx_throughput_lut::Generate(graphBuilder, context);

	coating_throughput_lut::Generate(graphBuilder, context);
}

static Bool Run(const lib::Path& outputDirectory)
{
	SPT_PROFILER_FUNCTION();

	std::filesystem::create_directories(outputDirectory);

	GenerationContext context(outputDirectory);

	tools::ExecuteRenderGraph([&context](rg::RenderGraphBuilder& graphBuilder)
							  {
								  GenerateMultiscatteringLUTs(graphBuilder, context);
							  });

	Bool success = true;
	for (js::JobWithResult<Bool>& saveJob : context.saveJobs)
	{
		success &= saveJob.Await();
	}

	if (!success)
	{
		SPT_LOG_ERROR(GGXMultiscatteringLUTsGenerator, "Failed to save roughness view LUT or E_avg LUT");
		return false;
	}

	context.saveJobs.clear();

	const lib::SharedPtr<rdr::Texture> Eavg = gfx::TextureLoader::LoadTexture((outputDirectory / "GGX_EAvg_LUT.dds").generic_string());
	const lib::SharedPtr<rdr::Texture> E     = gfx::TextureLoader::LoadTexture((outputDirectory / "GGX_E_LUT.dds").generic_string());

	gfx::GlobalShaderResources globalShaderResources = gfx::GlobalShaderResourcesManager::Get().GetResources();
	globalShaderResources.MS_GGX_E_avg_LUT = Eavg->CreateView(RENDERER_RESOURCE_NAME_FORMATTED("Global MS_GGX_E_avg_LUT"));
	globalShaderResources.MS_GGX_E_LUT     = E->CreateView(RENDERER_RESOURCE_NAME_FORMATTED("Global MS_GGX_E_LUT"));

	gfx::GlobalShaderResourcesManager::Get().UpdateResources(globalShaderResources);

	tools::ExecuteRenderGraph([&context](rg::RenderGraphBuilder& graphBuilder)
							  {
								  GenerateThroughputLUT(graphBuilder, context);
							  });

	for (js::JobWithResult<Bool>& saveJob : context.saveJobs)
	{
		success &= saveJob.Await();
	}

	return success;
}

} // spt::lut_gen


int main()
{
	using namespace spt;

	const tools::ScopedEngine engine;

	const engn::CommandLineArguments& cmdLineArgs = engn::Engine::Get().GetCmdLineArgs();

	const lib::Path outputDirectory = cmdLineArgs.Contains("-OutputPath")
		? lib::Path(cmdLineArgs.GetValue("-OutputPath").ToString())
		: engn::Engine::Get().GetPaths().savedPath / "LUTs";

	SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "Generating LUTs to: {}", outputDirectory.generic_string());

	const Bool success = lut_gen::Run(outputDirectory);

	if (success)
	{
		SPT_LOG_INFO(GGXMultiscatteringLUTsGenerator, "LUTs generation finished");
	}
	else
	{
		SPT_LOG_ERROR(GGXMultiscatteringLUTsGenerator, "LUTs generation failed");
	}

	return success ? 0 : 1;
}
