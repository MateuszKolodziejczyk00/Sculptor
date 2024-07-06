#ifndef SAMPLING_HLSLI
#define SAMPLING_HLSLI

template<typename TDataType>
TDataType SampleCatmullRom(Texture2D<TDataType> texture, SamplerState textureSampler, float2 uv, float2 resolution)
{
	// We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
	// down the sample location to get the exact center of our "starting" texel. The starting texel will be at
	// location [1, 1] in the grid, where [0, 0] is the top left corner.
	const float2 samplePosition = uv * resolution;
	const float2 texPos1 = floor(samplePosition - 0.5f) + 0.5f;

	// Compute the fractional offset from our starting texel to our original sample location, which we'll
	// feed into the Catmull-Rom spline function to get our filter weights.
	const float2 f = samplePosition - texPos1;

	// Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
	// These equations are pre-expanded based on our knowledge of where the texels will be located,
	// which lets us avoid having to evaluate a piece-wise function.
	const float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
	const float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
	const float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
	const float2 w3 = f * f * (-0.5f + 0.5f * f);

	// Work out weighting factors and sampling offsets that will let us use bilinear filtering to
	// simultaneously evaluate the middle 2 samples from the 4x4 grid.
	const float2 w12 = w1 + w2;
	const float2 offset_12 = w2 / (w1 + w2);

	// Compute the final UV coordinates we'll use for sampling the texture
	float2 texPos0 = texPos1 - 1;
	float2 texPos3 = texPos1 + 2;
	float2 texPos12 = texPos1 + offset_12;

	texPos0 /= resolution;
	texPos3 /= resolution;
	texPos12 /= resolution;

	TDataType result = 0.f;
	
	result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos0.y), 0) * w0.x * w0.y;
	result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos0.y), 0) * w12.x * w0.y;
	result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos0.y), 0) * w3.x * w0.y;
	
	result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos12.y), 0) * w0.x * w12.y;
	result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos12.y), 0) * w12.x * w12.y;
	result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos12.y), 0) * w3.x * w12.y;

	result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos3.y), 0) * w0.x * w3.y;
	result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos3.y), 0) * w12.x * w3.y;
	result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos3.y), 0) * w3.x * w3.y;

	return result;
}

// https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
// Daniel Ruijters and Philippe Thévenaz,
// GPU Prefilter for Accurate Cubic B-Spline Interpolation, 
// The Computer Journal, vol. 55, no. 1, pp. 15-20, January 2012.
// http://dannyruijters.nl/docs/cudaPrefilter3.pdf
template<typename TDataType>
TDataType SampleTricubic(in Texture3D<TDataType> texture, in SamplerState textureSampler, in float3 uvw, in float3 resolution)
{
   // shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
	float3 nrOfVoxels = resolution;
	float3 coord_grid = uvw * nrOfVoxels - 0.5;
	float3 index = floor(coord_grid);
	float3 fraction = coord_grid - index;
	float3 one_frac = 1.0 - fraction;

	float3 w0 = 1.0 / 6.0 * one_frac * one_frac * one_frac;
	float3 w1 = 2.0 / 3.0 - 0.5 * fraction * fraction * (2.0 - fraction);
	float3 w2 = 2.0 / 3.0 - 0.5 * one_frac * one_frac * (2.0 - one_frac);
	float3 w3 = 1.0 / 6.0 * fraction * fraction * fraction;

	float3 g0 = w0 + w1;
	float3 g1 = w2 + w3;
	float3 mult = 1.0 / nrOfVoxels;
	float3 h0 = mult * ((w1 / g0) - 0.5 + index);  //h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
	float3 h1 = mult * ((w3 / g1) + 1.5 + index);  //h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]

   // fetch the eight linear interpolations
   // weighting and fetching is interleaved for performance and stability reasons
	TDataType tex000 = texture.SampleLevel(textureSampler, h0, 0.f);
	TDataType tex100 = texture.SampleLevel(textureSampler, float3(h1.x, h0.y, h0.z), 0.f);
	tex000 = lerp(tex100, tex000, g0.x);  //weigh along the x-direction
	TDataType tex010 = texture.SampleLevel(textureSampler, float3(h0.x, h1.y, h0.z), 0.f);
	TDataType tex110 = texture.SampleLevel(textureSampler, float3(h1.x, h1.y, h0.z), 0.f);
	tex010 = lerp(tex110, tex010, g0.x);  //weigh along the x-direction
	tex000 = lerp(tex010, tex000, g0.y);  //weigh along the y-direction
	TDataType tex001 = texture.SampleLevel(textureSampler, float3(h0.x, h0.y, h1.z), 0.f);
	TDataType tex101 = texture.SampleLevel(textureSampler, float3(h1.x, h0.y, h1.z), 0.f);
	tex001 = lerp(tex101, tex001, g0.x);  //weigh along the x-direction
	TDataType tex011 = texture.SampleLevel(textureSampler, float3(h0.x, h1.y, h1.z), 0.f);
	TDataType tex111 = texture.SampleLevel(textureSampler, h1, 0.f);
	tex011 = lerp(tex111, tex011, g0.x);  //weigh along the x-direction
	tex001 = lerp(tex011, tex001, g0.y);  //weigh along the y-direction

	return lerp(tex001, tex000, g0.z);  //weigh along the z-direction
}

#endif // SAMPLING_HLSLI
