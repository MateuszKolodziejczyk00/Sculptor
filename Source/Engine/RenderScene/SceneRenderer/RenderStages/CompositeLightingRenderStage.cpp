#include "CompositeLightingRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
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
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/RTReflectionsTypes.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"


namespace spt::rsc
{

namespace composite_pass_impl
{

BEGIN_SHADER_STRUCT(CompositeLightingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
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


DS_BEGIN(CompositeRTReflectionsDS, rg::RGDescriptorSetState<CompositeRTReflectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>), u_reflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),          u_reflectionsInfluenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_baseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),         u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_tangentFrameTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),          u_reservoirsBrightnessTexture)
DS_END();


struct CompositeLightingPermutation
{
	Bool volumetricFogEnabled = false;
	Bool atmosphereEnabled    = false;
	Bool rtReflectionsEnabled = false;
};


static rdr::ShaderID CompileCompositeLightingShader(const CompositeLightingPermutation& permutation)
{
	sc::ShaderCompilationSettings shaderCompilationSettings;
	shaderCompilationSettings.AddMacroDefinition(sc::MacroDefinition("VOLUMETRIC_FOG_ENABLED", permutation.volumetricFogEnabled ? "1" : "0"));
	shaderCompilationSettings.AddMacroDefinition(sc::MacroDefinition("ATMOSPHERE_ENABLED", permutation.atmosphereEnabled ? "1" : "0"));
	shaderCompilationSettings.AddMacroDefinition(sc::MacroDefinition("RT_REFLECTIONS_ENABLED", permutation.rtReflectionsEnabled ? "1" : "0"));

	return rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/CompositeLighting/CompositeLighting.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CompositeLightingCS"), shaderCompilationSettings);
}


static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	CompositeLightingConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();

	const lib::MTHandle<CompositeLightingDS> compositeLightingDS = graphBuilder.CreateDescriptorSet<CompositeLightingDS>(RENDERER_RESOURCE_NAME("CompositeLightingDS"));
	compositeLightingDS->u_luminanceTexture = viewContext.luminance;
	compositeLightingDS->u_depthTexture     = viewContext.depth;
	compositeLightingDS->u_constants        = shaderConstants;

	CompositeLightingPermutation permutation;

	lib::MTHandle<CompositeFogDS> compositeFogDS;
	if (const lib::SharedPtr<ParticipatingMediaViewRenderSystem> participatingMediaSystem = renderView.FindRenderSystem<ParticipatingMediaViewRenderSystem>())
	{
		permutation.volumetricFogEnabled = true;

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
	if (lib::SharedPtr<AtmosphereSceneSubsystem> atmosphereSubsystem = renderScene.GetSceneSubsystem<AtmosphereSceneSubsystem>())
	{
		permutation.atmosphereEnabled = true;

		const AtmosphereContext& atmosphereContext = atmosphereSubsystem->GetAtmosphereContext();

		compositeAtmosphereDS = graphBuilder.CreateDescriptorSet<CompositeAtmosphereDS>(RENDERER_RESOURCE_NAME("CompositeAtmosphereDS"));
		compositeAtmosphereDS->u_atmosphereParams  = atmosphereContext.atmosphereParamsBuffer->CreateFullView();
		compositeAtmosphereDS->u_directionalLights = atmosphereContext.directionalLightsBuffer->CreateFullView();
		compositeAtmosphereDS->u_transmittanceLUT  = atmosphereContext.transmittanceLUT;
		compositeAtmosphereDS->u_skyViewLUT        = viewContext.skyViewLUT;
	}

	lib::MTHandle<CompositeRTReflectionsDS> compositeRTReflectionsDS;
	if (const RTReflectionsViewData* rtReflectionsData = viewSpec.GetData().Find<RTReflectionsViewData>())
	{
		permutation.rtReflectionsEnabled = true;

		compositeRTReflectionsDS = graphBuilder.CreateDescriptorSet<CompositeRTReflectionsDS>(RENDERER_RESOURCE_NAME("CompositeRTReflectionsDS"));
		compositeRTReflectionsDS->u_reflectionsTexture          = rtReflectionsData->reflectionsTexture;
		compositeRTReflectionsDS->u_reflectionsInfluenceTexture = rtReflectionsData->reflectionsInfluenceTexture;
		compositeRTReflectionsDS->u_brdfIntegrationLUT          = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
		compositeRTReflectionsDS->u_baseColorMetallicTexture    = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		compositeRTReflectionsDS->u_roughnessTexture            = viewContext.gBuffer[GBuffer::Texture::Roughness];
		compositeRTReflectionsDS->u_tangentFrameTexture         = viewContext.gBuffer[GBuffer::Texture::TangentFrame];
		compositeRTReflectionsDS->u_reservoirsBrightnessTexture = rtReflectionsData->reservoirsBrightnessTexture;
	}

	const rdr::ShaderID compositeLightingShader = CompileCompositeLightingShader(permutation);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Composite Lighting"),
						  compositeLightingShader,
						  math::Utils::DivideCeil(renderView.GetRenderingRes(), math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(compositeLightingDS),
												 std::move(compositeFogDS),
												 std::move(compositeAtmosphereDS),
												 std::move(compositeRTReflectionsDS),
												 renderView.GetRenderViewDS()));
}


} // composite_pass_impl

REGISTER_RENDER_STAGE(ERenderStage::CompositeLighting, CompositeLightingRenderStage);

CompositeLightingRenderStage::CompositeLightingRenderStage()
{ }

void CompositeLightingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	composite_pass_impl::Render(graphBuilder, renderScene, viewSpec);

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
