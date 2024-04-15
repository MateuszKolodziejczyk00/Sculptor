#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(ApplySpecularReflectionsDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplySpecularReflectionsCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 uv = float2(pixel + 0.5f) / float2(outputRes);

		float3 luminance = u_luminanceTexture[pixel].rgb;

		const float3 specularReflections = u_specularReflectionsTexture.Load(uint3(pixel, 0)).rgb;

		if(any(isnan(specularReflections)))
		{
			return;
		}

		const float4 specularColorAndRoughness = u_specularAndRoughnessTexture.Load(uint3(pixel, 0));

		const float3 specularColor = specularColorAndRoughness.rgb;
		const float roughness      = specularColorAndRoughness.a;

		const float3 viewLocation = u_sceneView.viewLocation;

		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		const float3 worldLocation = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, depth), u_sceneView);

		const float3 viewDir = normalize(viewLocation - worldLocation);
		const float3 normal  = u_normalsTexture.Load(uint3(pixel, 0)).rgb * 2.f - 1.f;

		const float NdotV = saturate(dot(normal, viewDir));

		const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_linearSampler, float2(NdotV, roughness), 0);

		luminance += specularReflections * (specularColor * integratedBRDF.x + integratedBRDF.y);

		u_luminanceTexture[pixel] = float4(luminance, 1.f);
	}
}
