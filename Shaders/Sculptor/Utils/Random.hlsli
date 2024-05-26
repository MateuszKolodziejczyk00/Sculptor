#ifndef RANDOM_HLSLI
#define RANDOM_HLSLI

float3 VectorInCone(float3 coneDir, float coneHalfAngleRad, float2 random)
{
	// Compute the basis vectors for the cone
	const float3 u = normalize(cross(coneDir, float3(0.f, 1.f, 0.f)));
	const float3 v = normalize(cross(coneDir, u));
	
	const float phi = 2.f * PI * random.y;

	const float minZ = cos(coneHalfAngleRad);
	const float z = lerp(minZ, 1.f, random.x);

	const float theta = acos(z);
	
	// Compute the random vector in spherical coordinates
	const float3 randomVector = sin(theta) * (cos(phi) * u + sin(phi) * v) + cos(theta) * coneDir;
	
	// Normalize the vector and return it
	return normalize(randomVector);
}


// Based on: Chapter 16.6.1 of "Ray Tracing Gems"
float3 RandomVectorInCosineWeightedHemisphere(in float3x3 tangent, in float2 random, out float pdf)
{
	const float a2 = random.x;
	const float a = sqrt(a2);
	const float phi = 2.f * PI * random.y;
	float sinPhi, cosPhi;
	sincos(phi, OUT sinPhi, OUT cosPhi);

	const float x = a * cosPhi;
	const float y = a * sinPhi;
	const float z = sqrt(1.f - a2);

	const float3 direction = mul(tangent, float3(x, y, z));
	pdf = z/ PI;

	return direction;
}


// Based on: Chapter 16.6.2 of "Ray Tracing Gems"
float3 RandomVectorInCosineWeightedHemisphere(in float3 direction, in float2 random, out float pdf)
{
	const float a = 1.f - 2.f * random.x;
	const float b = sqrt(1.f - Pow2(a));
	const float phi = 2.f * PI * random.y;

	float sinPhi, cosPhi;
	sincos(phi, OUT sinPhi, OUT cosPhi);

	const float x = direction.x + b * cosPhi;
	const float y = direction.y + b * sinPhi;
	const float z = direction.z + a;

	pdf = a / PI;

	return float3(x, y, z);
}

// Source: "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
float InterleavedGradientNoise(float2 uv)
{
	const float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(uv, magic.xy)));
}

float2x2 NoiseRotation(float noise)
{
	float sin;
	float cos;
	sincos(2.f * PI * noise, sin, cos);

	return float2x2(cos, -sin,
					sin, cos);
}

float2x2 InterleavedGradientNoiseRotation(float2 uv)
{
	return NoiseRotation(InterleavedGradientNoise(uv));
}


float Random(float2 seed)
{
	return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}


uint PCGHash(in uint input)
{
	uint state = input * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}


float2 PCGHashFloat2(in uint2 pixel, in uint frame)
{
	const uint state = pixel.x * 747796405u + 2891336453u + pixel.y * 277803737u + frame * 747796405u;
	const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return float2((word >> 22u) ^ word, (word >> 7u) ^ word);
}


class RngState
{
	static RngState Create(in float2 seed)
	{
		RngState state;
		state.seed = seed;
		return state;
	}

	float Next()
	{
		seed = Random(seed);
		return seed.x;
	}

	float2 seed;
};

#endif // RANDOM_HLSLI
