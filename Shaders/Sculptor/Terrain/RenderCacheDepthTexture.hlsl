#include "SculptorShader.hlsli"

[[shader_params(RenderCacheDepthTextureConstants , PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS)]]

#include "Utils/FullScreen.hlsli"


struct PS_OUTPUT
{
	float depth : SV_Depth;
};


bool IsWithinBounds(float2 location, float2 minBounds, float2 maxBounds)
{
	return all(location >= minBounds) && all(location <= maxBounds);
}


PS_OUTPUT RenderCacheDepthTextureFS(VS_OUTPUT input)
{
	const float2 lodMinUV = PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->newMinBounds / PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->rcpLodRange;
	const float2 lodUV = frac(input.uv - lodMinUV);

	const float2 worldLocation = PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->newMinBounds + lodUV * PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->rcpLodRange;

	const bool inPrevBounds = IsWithinBounds(worldLocation, PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->prevMinBounds, PARAMS_RENDER_CACHE_DEPTH_TEXTURE_CONSTANTS->prevMaxBounds);

	PS_OUTPUT output;
	output.depth = inPrevBounds ? 0.f : 1.0f;

	return output;
}
