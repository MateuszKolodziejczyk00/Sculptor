#include "SculptorShader.hlsli"

[[shader_params(GeneratePBRTexturesMipsConstants, u_params)]]

#include "Utils/ColorSpaces.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


template<typename T>
void DownsampleLinear(in SRVTexture2D<T> inputTexture, in UAVTexture2D<T> outputTexture, in int2 outputCoords)
{
	if (any(outputCoords >= outputTexture.GetResolution()))
	{
		return;
	}

	const float2 uv = (float2(outputCoords) + 0.5f) / outputTexture.GetResolution().xy;

	const uint2 inputRes = inputTexture.GetResolution();

	T sum = (T)0.f;
	float weightSum = 0.f;

	const float2 inputCoords = uv * inputRes;

	// manual linear sample with sRGB to linear conversion
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 2; ++x)
		{
			const int2 sampleCoords = min(int2(inputCoords) + int2(x, y), inputRes - 1);

			const T sample = inputTexture.Load(sampleCoords);
			float weight = 1.f - abs(frac(inputCoords.x) - x) * (1.f - abs(frac(inputCoords.y) - y));

			if (u_params.inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = u_params.inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
				weight *= alpha;
			}

			sum += sample * weight;
			weightSum += weight;
		}
	}

	const T result = weightSum > 0.f ? sum / weightSum : (T)0.f;
	outputTexture.Store(outputCoords, result);
}


void DownsampleSRGB(in SRVTexture2D<float4> inputTexture, in UAVTexture2D<float4> outputTexture, in int2 outputCoords)
{
	if (any(outputCoords >= outputTexture.GetResolution()))
	{
		return;
	}

	const float2 uv = (float2(outputCoords) + 0.5f) / outputTexture.GetResolution().xy;

	const uint2 inputRes = inputTexture.GetResolution();

	float4 sum = 0.f;
	float weightSum = 0.f;

	const float2 inputCoords = uv * inputRes;

	// manual linear sample with sRGB to linear conversion
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 2; ++x)
		{
			const int2 sampleCoords = min(int2(inputCoords) + int2(x, y), inputRes - 1);

			const float4 sample = inputTexture.Load(sampleCoords);
			float weight = 1.f - abs(frac(inputCoords.x) - x) * (1.f - abs(frac(inputCoords.y) - y));

			if (u_params.inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = u_params.inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
				weight *= alpha;
			}

			sum += float4(SRGBToLinear(sample.xyz), sample.w) * weight;
			weightSum += weight;
		}
	}

	outputTexture.Store(outputCoords, float4(LinearTosRGB(sum.xyz / weightSum), sum.w / weightSum));
}


void DownsampleNormals(in SRVTexture2D<float2> inputTexture, in UAVTexture2D<float2> outputTexture, in int2 outputCoords)
{
	if (any(outputCoords >= outputTexture.GetResolution()))
	{
		return;
	}

	const float2 uv = (float2(outputCoords) + 0.5f) / outputTexture.GetResolution().xy;

	const uint2 inputRes = inputTexture.GetResolution();

	float3 sum = 0.f;
	float weightSum = 0.f;

	const float2 inputCoords = uv * inputRes;

	// manual linear sample with sRGB to linear conversion
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 2; ++x)
		{
			const int2 sampleCoords = min(int2(inputCoords) + int2(x, y), inputRes - 1);

			const float2 sample = inputTexture.Load(sampleCoords);
			float weight = 1.f - abs(frac(inputCoords.x) - x) * (1.f - abs(frac(inputCoords.y) - y));

			if (u_params.inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = u_params.inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
				weight *= alpha;
			}

			sum += UnpackTangentNormalFromXY(sample) * weight;
			weightSum += weight;
		}
	}

	const float3 avgNormal = weightSum > 0.f ? normalize(sum / weightSum) : float3(0.f, 0.f, 1.f);
	outputTexture.Store(outputCoords, PackTangentNormalToXY(avgNormal));
}


[numthreads(16, 16, 1)]
void GeneratePBRTexturesMipsCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	float4 alphaValues = 1.f;
	if (u_params.rwAlpha.IsValid())
	{
		const float2 alphaUV = (float2(coords) + 0.5f) / u_params.rwAlpha.GetResolution().xy;
		const float outAlpha = dot(u_params.inAlpha.Gather<float4>(BindlessSamplers::LinearClampEdge(), alphaUV), 0.25f);
		u_params.rwAlpha.Store(coords, outAlpha);
	}

	if (u_params.rwBaseColor.IsValid())
	{
		DownsampleSRGB(u_params.inBaseColor, u_params.rwBaseColor, coords);
	}

	if (u_params.rwMetallicRoughness.IsValid())
	{
		DownsampleLinear(u_params.inMetallicRoughness, u_params.rwMetallicRoughness, coords);
	}

	if (u_params.rwNormals.IsValid())
	{
		DownsampleNormals(u_params.inNormals, u_params.rwNormals, coords);
	}

	if (u_params.rwEmissive.IsValid())
	{
		DownsampleSRGB(u_params.inEmissive, u_params.rwEmissive, coords);
	}
}
