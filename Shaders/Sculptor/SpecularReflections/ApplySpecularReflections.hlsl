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

		const float4 baseColorMetallic = u_baseColorMetallicTexture.Load(uint3(pixel, 0));

		float3 diffuseColor;
		float3 specularColor;
		
		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

		luminance += specularReflections * specularColor;

		u_luminanceTexture[pixel] = float4(luminance, 1.f);
	}
}
