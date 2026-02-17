#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[shader_params(ResolveStochasticDIConstants, u_constants)]]

#include "Utils/GBuffer/GBuffer.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveStochasticDICS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const GBufferInterface gBuffer = GBufferInterface(u_constants.gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 demodulatedSpecularLo = u_constants.specular.Load(coords);
	const float3 demodulatedDiffuseLo  = u_constants.diffuse.Load(coords);

	const float3 diffuseLo = demodulatedDiffuseLo * surface.diffuseColor;

	const float3 V = normalize(u_sceneView.viewLocation - surface.location);

	const float NdotV = saturate(dot(surface.normal, V));
	const float2 integratedBRDF = u_constants.brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, surface.roughness), 0);
	const float3 specularLo = demodulatedSpecularLo * (surface.specularColor * integratedBRDF.x + integratedBRDF.y);

	const float3 prevLo = u_constants.rwLuminance.Load(coords);

	const float3 finalLo = prevLo + diffuseLo + specularLo;

	u_constants.rwLuminance.Store(coords, finalLo);
}
