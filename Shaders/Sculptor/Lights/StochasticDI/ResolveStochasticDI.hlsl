#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(ResolveStochasticDIConstants, PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS)]]

#include "Utils/GBuffer/GBuffer.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveStochasticDICS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const GBufferInterface gBuffer = GBufferInterface(PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 demodulatedSpecularLo = PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->specular.Load(coords);
	const float3 demodulatedDiffuseLo  = PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->diffuse.Load(coords);

	const float3 diffuseLo = demodulatedDiffuseLo * surface.diffuseColor;

	const float3 V = normalize(VIEW->sceneView.viewLocation - surface.location);

	const float NdotV = saturate(dot(surface.normal, V));
	const float2 integratedBRDF = PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, surface.roughness), 0);
	const float3 specularLo = demodulatedSpecularLo * (surface.specularColor * integratedBRDF.x + integratedBRDF.y);

	const float3 prevLo = PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->rwLuminance.Load(coords);

	const float3 finalLo = prevLo + diffuseLo + specularLo;

	PARAMS_RESOLVE_STOCHASTIC_D_I_CONSTANTS->rwLuminance.Store(coords, finalLo);
}
