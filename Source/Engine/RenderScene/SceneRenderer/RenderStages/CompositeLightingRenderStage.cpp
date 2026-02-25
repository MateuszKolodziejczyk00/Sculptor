#include "CompositeLightingRenderStage.h"
#include "Atmosphere/AtmosphereRenderSystem.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "ResourcesManager.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "View/RenderView.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "Atmosphere/AtmosphereTypes.h"
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

BEGIN_SHADER_STRUCT(CompositeLightingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Bool,           enableColoredAO)
END_SHADER_STRUCT();


DS_BEGIN(CompositeLightingDS, rg::RGDescriptorSetState<CompositeLightingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CompositeLightingConstants>),             u_constants)
DS_END();


DS_BEGIN(CompositeAtmosphereDS, rg::RGDescriptorSetState<CompositeAtmosphereDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),       u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>), u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),              u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),              u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),              u_aerialPerspective)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),      u_volumetricClouds)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),              u_volumetricCloudsDepth)
DS_END();


BEGIN_SHADER_STRUCT(CompositeFogConstants)
	SHADER_STRUCT_FIELD(math::Vector3f, fogResolution)
	SHADER_STRUCT_FIELD(Real32,         fogNearPlane)
	SHADER_STRUCT_FIELD(Real32,         fogFarPlane)
END_SHADER_STRUCT();


DS_BEGIN(CompositeFogDS, rg::RGDescriptorSetState<CompositeFogDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),          u_integratedInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CompositeFogConstants>), u_fogParams)
DS_END();


BEGIN_SHADER_STRUCT(CompositeRTReflectionsConstants)
	SHADER_STRUCT_FIELD(Uint32, halfResInfluence)
	SHADER_STRUCT_FIELD(Uint32, aoEnabled)
END_SHADER_STRUCT();


DS_BEGIN(CompositeRTReflectionsDS, rg::RGDescriptorSetState<CompositeRTReflectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_specularGI)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_diffuseGI)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                    u_ambientOcclusion)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                     u_reflectionsInfluenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                    u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                    u_baseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                            u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                    u_tangentFrameTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CompositeRTReflectionsConstants>), u_rtReflectionsConstants)
DS_END();


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


static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	CompositeLightingConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.enableColoredAO = renderer_params::enableColoredAO;

	const lib::MTHandle<CompositeLightingDS> compositeLightingDS = graphBuilder.CreateDescriptorSet<CompositeLightingDS>(RENDERER_RESOURCE_NAME("CompositeLightingDS"));
	compositeLightingDS->u_luminanceTexture = viewContext.luminance;
	compositeLightingDS->u_depthTexture     = viewContext.depth;
	compositeLightingDS->u_constants        = shaderConstants;

	CompositeLightingPermutation permutation;

	lib::MTHandle<CompositeFogDS> compositeFogDS;
	if (const lib::SharedPtr<ParticipatingMediaViewRenderSystem> participatingMediaSystem = renderView.FindRenderSystem<ParticipatingMediaViewRenderSystem>())
	{
		permutation.VOLUMETRIC_FOG_ENABLED = true;

		const VolumetricFogParams& fogParams = participatingMediaSystem->GetVolumetricFogParams();

		CompositeFogConstants fogConstants;
		fogConstants.fogResolution = fogParams.volumetricFogResolution.cast<Real32>();
		fogConstants.fogNearPlane  = fogParams.nearPlane;
		fogConstants.fogFarPlane   = fogParams.farPlane;

		compositeFogDS = graphBuilder.CreateDescriptorSet<CompositeFogDS>(RENDERER_RESOURCE_NAME("CompositeFogDS"));
		compositeFogDS->u_integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;
		compositeFogDS->u_fogParams                     = fogConstants;
	}

	lib::MTHandle<CompositeAtmosphereDS> compositeAtmosphereDS;
	if (lib::SharedPtr<AtmosphereRenderSystem> atmosphereRenderSystem = renderScene.FindRenderSystem<AtmosphereRenderSystem>())
	{
		permutation.ATMOSPHERE_ENABLED = true;

		const AtmosphereContext& atmosphereContext = atmosphereRenderSystem->GetAtmosphereContext();

		compositeAtmosphereDS = graphBuilder.CreateDescriptorSet<CompositeAtmosphereDS>(RENDERER_RESOURCE_NAME("CompositeAtmosphereDS"));
		compositeAtmosphereDS->u_atmosphereParams      = atmosphereContext.atmosphereParamsBuffer->GetFullView();
		compositeAtmosphereDS->u_directionalLights     = atmosphereContext.directionalLightsBuffer->GetFullView();
		compositeAtmosphereDS->u_transmittanceLUT      = atmosphereContext.transmittanceLUT;
		compositeAtmosphereDS->u_skyViewLUT            = viewContext.skyViewLUT;
		compositeAtmosphereDS->u_aerialPerspective     = viewContext.aerialPerspective;

		if (viewContext.volumetricClouds.IsValid())
		{
			permutation.VOLUMETRIC_CLOUDS_ENABLED = true;

			compositeAtmosphereDS->u_volumetricClouds      = viewContext.volumetricClouds;
			compositeAtmosphereDS->u_volumetricCloudsDepth = viewContext.volumetricCloudsDepth;
		}
	}

	lib::MTHandle<CompositeRTReflectionsDS> compositeRTReflectionsDS;
	if (const RTReflectionsViewData* rtReflectionsData = viewSpec.GetBlackboard().Find<RTReflectionsViewData>())
	{
		permutation.RT_REFLECTIONS_ENABLED = true;

		CompositeRTReflectionsConstants rtReflectionsConstants;
		rtReflectionsConstants.halfResInfluence = rtReflectionsData->halfResReflections;

		compositeRTReflectionsDS = graphBuilder.CreateDescriptorSet<CompositeRTReflectionsDS>(RENDERER_RESOURCE_NAME("CompositeRTReflectionsDS"));
		compositeRTReflectionsDS->u_specularGI                  = rtReflectionsData->finalSpecularGI;
		compositeRTReflectionsDS->u_diffuseGI                   = rtReflectionsData->finalDiffuseGI;
		compositeRTReflectionsDS->u_reflectionsInfluenceTexture = rtReflectionsData->reflectionsInfluenceTexture;
		compositeRTReflectionsDS->u_brdfIntegrationLUT          = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
		compositeRTReflectionsDS->u_baseColorMetallicTexture    = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		compositeRTReflectionsDS->u_roughnessTexture            = viewContext.gBuffer[GBuffer::Texture::Roughness];
		compositeRTReflectionsDS->u_tangentFrameTexture         = viewContext.gBuffer[GBuffer::Texture::TangentFrame];

		if (viewContext.ambientOcclusion.IsValid())
		{
			compositeRTReflectionsDS->u_ambientOcclusion = viewContext.ambientOcclusion;
			rtReflectionsConstants.aoEnabled = 1u;
		}

		compositeRTReflectionsDS->u_rtReflectionsConstants = rtReflectionsConstants;
	}

	graphBuilder.Dispatch(RG_DEBUG_NAME("Composite Lighting"),
						  CompositeLightingPSO::GetPermutation(permutation),
						  math::Utils::DivideCeil(renderView.GetRenderingRes(), math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(compositeLightingDS),
												 std::move(compositeFogDS),
												 std::move(compositeAtmosphereDS),
												 std::move(compositeRTReflectionsDS)));
}


} // composite_pass_impl

REGISTER_RENDER_STAGE(ERenderStage::CompositeLighting, CompositeLightingRenderStage);

CompositeLightingRenderStage::CompositeLightingRenderStage()
{ }

void CompositeLightingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::VolumetricClouds).Broadcast(graphBuilder, renderScene, viewSpec, RenderViewEntryContext{});

	composite_pass_impl::Render(graphBuilder, renderScene, viewSpec);

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
