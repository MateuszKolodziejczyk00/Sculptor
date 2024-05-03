#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(ApplySpecularReflectionsDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/GBuffer.hlsli"
#include "Shading/Shading.hlsli"

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

		GBufferInput gBufferInput;
		gBufferInput.gBuffer0 = u_gBuffer0Texture;
		gBufferInput.gBuffer1 = u_gBuffer1Texture;
		gBufferInput.gBuffer2 = u_gBuffer2Texture;
		gBufferInput.gBuffer3 = u_gBuffer3Texture;
		gBufferInput.gBuffer4 = u_gBuffer4Texture;

		const GBufferData gBufferData = DecodeGBuffer(gBufferInput, int3(pixel, 0));

		float3 diffuseColor  = 0.f;
		float3 specularColor = 0.f;
		ComputeSurfaceColor(gBufferData.baseColor, gBufferData.metallic, OUT diffuseColor, OUT specularColor);

		const float3 viewLocation = u_sceneView.viewLocation;

		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		const float3 worldLocation = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, depth), u_sceneView);

		const float3 viewDir = normalize(viewLocation - worldLocation);

		const float NdotV = saturate(dot(gBufferData.normal, viewDir));

		const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_linearSampler, float2(NdotV, gBufferData.roughness), 0);

		luminance += specularReflections * (specularColor * integratedBRDF.x + integratedBRDF.y);

		u_luminanceTexture[pixel] = float4(luminance, 1.f);
	}
}
