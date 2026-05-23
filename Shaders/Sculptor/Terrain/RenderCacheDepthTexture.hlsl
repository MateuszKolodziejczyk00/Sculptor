#include "SculptorShader.hlsli"

[[shader_params(RenderCacheDepthTextureConstants , u_constants)]]

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
	const float2 lodMinUV = u_constants.newMinBounds / u_constants.rcpLodRange;
	const float2 lodUV = frac(input.uv - lodMinUV);

	const float2 worldLocation = u_constants.newMinBounds + lodUV * u_constants.rcpLodRange;

	const bool inPrevBounds = IsWithinBounds(worldLocation, u_constants.prevMinBounds, u_constants.prevMaxBounds);

	PS_OUTPUT output;
	output.depth = inPrevBounds ? 0.f : 1.0f;

	return output;
}
