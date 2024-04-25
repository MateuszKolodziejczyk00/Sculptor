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

#endif // SAMPLING_HLSLI
