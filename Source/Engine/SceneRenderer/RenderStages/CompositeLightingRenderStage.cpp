#include "CompositeLightingRenderStage.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "Utils/SceneRenderingTypes.h"
#include "View/RenderView.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereTypes.h"
#include "Lights/LightTypes.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/RTReflectionsTypes.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Pipelines/PSOsLibraryTypes.h"

namespace spt::rsc
{

namespace renderer_params
{

RendererBoolParameter enableColoredAO("Enable Colored AO", { "Lighting" }, true);

} // renderer_params

namespace composite_pass_impl
{


BEGIN_SHADER_STRUCT(CompositeAtmosphereConstants)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,             atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<DirectionalLightGPUData>, directionalLights)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,         transmittanceLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,         skyViewLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>,         aerialPerspective)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,         volumetricClouds)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,         cirrusClouds)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                 volumetricCloudsDepth)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(CompositeFogConstants)
	SHADER_STRUCT_FIELD(math::Vector3f,                    fogResolution)
	SHADER_STRUCT_FIELD(Real32,                            fogNearPlane)
	SHADER_STRUCT_FIELD(Real32,                            fogFarPlane)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, integratedInScatteringTexture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(CompositeRTReflectionsConstants)
	SHADER_STRUCT_FIELD(Uint32,                            halfResInfluence)
	SHADER_STRUCT_FIELD(Uint32,                            aoEnabled)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, specularGI)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, diffuseGI)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         ambientOcclusion)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, reflectionsInfluenceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, brdfIntegrationLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, baseColorMetallicTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, tangentFrameTexture)
END_SHADER_STRUCT();

BEGIN_SHADER_STRUCT(CompositeLightingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                               resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                               invResolution)
	SHADER_STRUCT_FIELD(Bool,                                         enableColoredAO)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                    depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                    aoTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>,            luminanceTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<CompositeFogConstants>,           fogParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<CompositeAtmosphereConstants>,    atmosphereParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<CompositeRTReflectionsConstants>, rtReflectionsParams)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(CompositeLightingPermutation)
	SHADER_STRUCT_FIELD(Bool, VOLUMETRIC_FOG_ENABLED)
	SHADER_STRUCT_FIELD(Bool, ATMOSPHERE_ENABLED)
	SHADER_STRUCT_FIELD(Bool, VOLUMETRIC_CLOUDS_ENABLED)
	SHADER_STRUCT_FIELD(Bool, RT_REFLECTIONS_ENABLED)
END_SHADER_STRUCT();


COMPUTE_PSO(CompositeLightingPSO)
{
	COMPUTE_SHADER("Sculptor/RenderStages/CompositeLighting/CompositeLighting.hlsl", CompositeLightingCS);

	PERMUTATION_DOMAIN(CompositeLightingPermutation);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		CompositeLightingPermutation perm;
		perm.VOLUMETRIC_CLOUDS_ENABLED = true;
		perm.ATMOSPHERE_ENABLED        = true;
		perm.VOLUMETRIC_FOG_ENABLED    = true;
		perm.RT_REFLECTIONS_ENABLED    = rdr::GPUApi::IsRayTracingEnabled();

		CompilePermutation(compiler, perm);
	}
};


static void Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	CompositeLightingConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.enableColoredAO = renderer_params::enableColoredAO;
	shaderConstants.luminanceTexture = viewContext.luminance;
	shaderConstants.depthTexture     = viewContext.depth;
	shaderConstants.aoTexture        = viewContext.gBuffer[GBuffer::Texture::Occlusion];

	CompositeLightingPermutation permutation;

	if (const ParticipatingMediaViewRenderSystem* participatingMediaSystem = viewSpec.GetRenderSystem<ParticipatingMediaViewRenderSystem>())
	{
		permutation.VOLUMETRIC_FOG_ENABLED = true;

		const VolumetricFogParams& fogParams = participatingMediaSystem->GetVolumetricFogParams();

		CompositeFogConstants fogConstants;
		fogConstants.fogResolution                 = fogParams.volumetricFogResolution.cast<Real32>();
		fogConstants.fogNearPlane                  = fogParams.nearPlane;
		fogConstants.fogFarPlane                   = fogParams.farPlane;
		fogConstants.integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;

		shaderConstants.fogParams = graphBuilder.CreateGPUData(fogConstants);
	}

	if (const AtmosphereRenderSystem* atmosphereRenderSystem = rendererInterface.GetRenderSystem<AtmosphereRenderSystem>())
	{
		permutation.ATMOSPHERE_ENABLED = true;

		const AtmosphereContext& atmosphereContext = atmosphereRenderSystem->GetAtmosphereContext();

		CompositeAtmosphereConstants atmosphereConstants;
		atmosphereConstants.atmosphereParams      = atmosphereContext.atmosphereParams;
		atmosphereConstants.directionalLights     = atmosphereContext.directionalLightsBuffer->GetFullView();
		atmosphereConstants.transmittanceLUT      = atmosphereContext.transmittanceLUT;
		atmosphereConstants.skyViewLUT            = viewContext.skyViewLUT;
		atmosphereConstants.aerialPerspective     = viewContext.aerialPerspective;

		if (viewContext.volumetricClouds.IsValid())
		{
			permutation.VOLUMETRIC_CLOUDS_ENABLED = true;

			atmosphereConstants.volumetricClouds      = viewContext.volumetricClouds;
			atmosphereConstants.cirrusClouds          = viewContext.cirrusClouds;
			atmosphereConstants.volumetricCloudsDepth = viewContext.volumetricCloudsDepth;
		}

		shaderConstants.atmosphereParams = graphBuilder.CreateGPUData(atmosphereConstants);
	}

	if (const RTReflectionsViewData* rtReflectionsData = viewSpec.GetBlackboard().Find<RTReflectionsViewData>())
	{
		permutation.RT_REFLECTIONS_ENABLED = true;

		CompositeRTReflectionsConstants rtReflectionsConstants;
		rtReflectionsConstants.halfResInfluence = rtReflectionsData->halfResReflections;
		rtReflectionsConstants.specularGI                  = rtReflectionsData->finalSpecularGI;
		rtReflectionsConstants.diffuseGI                   = rtReflectionsData->finalDiffuseGI;
		rtReflectionsConstants.reflectionsInfluenceTexture = rtReflectionsData->reflectionsInfluenceTexture;
		rtReflectionsConstants.brdfIntegrationLUT          = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
		rtReflectionsConstants.baseColorMetallicTexture    = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		rtReflectionsConstants.roughnessTexture            = viewContext.gBuffer[GBuffer::Texture::Roughness];
		rtReflectionsConstants.tangentFrameTexture         = viewContext.gBuffer[GBuffer::Texture::TangentFrame];

		if (viewContext.ambientOcclusion.IsValid())
		{
			rtReflectionsConstants.ambientOcclusion = viewContext.ambientOcclusion;
			rtReflectionsConstants.aoEnabled = 1u;
		}

		shaderConstants.rtReflectionsParams = graphBuilder.CreateGPUData(rtReflectionsConstants);
	}

	graphBuilder.Dispatch(RG_DEBUG_NAME("Composite Lighting"),
						  CompositeLightingPSO::GetPermutation(permutation),
						  math::Utils::DivideCeil(viewSpec.GetRenderingRes(), math::Vector2u(8u, 4u)),
						  rg::ShaderParams(shaderConstants));
}

} // composite_pass_impl

REGISTER_RENDER_STAGE(ERenderStage::CompositeLighting, CompositeLightingRenderStage);

CompositeLightingRenderStage::CompositeLightingRenderStage()
{ }

void CompositeLightingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderVolumetricClouds).Broadcast(graphBuilder, rendererInterface,renderScene, viewSpec, RenderViewEntryContext{});

	composite_pass_impl::Render(graphBuilder, rendererInterface, renderScene, viewSpec);

	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderVariableRateTexture).Broadcast(graphBuilder, rendererInterface,renderScene, viewSpec, RenderViewEntryContext{});
}

} // spt::rsc
