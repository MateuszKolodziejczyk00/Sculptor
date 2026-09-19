#include "SculptorShader.hlsli"

[[shader_params(GeneratePBRTexturesMipsConstants, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS)]]

#include "Utils/ColorSpaces.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


void DownsampleLinear<T : ITexelElement & IFloat>(in SRVTexture2D<T> inputTexture, in UAVTexture2D<T> outputTexture, in int2 outputCoords)
{
	if (any(outputCoords >= outputTexture.GetResolution()))
	{
		return;
	}

	const float2 uv = (float2(outputCoords) + 0.5f) / outputTexture.GetResolution().xy;

	const uint2 inputRes = inputTexture.GetResolution();

	T sum = {};
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

			if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0.f);
				weight *= alpha;
			}

			sum = sum + sample * (T)weight;
			weightSum += weight;
		}
	}

	const T result = weightSum > 0.f ? sum / (T)weightSum : DefaultValue<T>();
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

			if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
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

			if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) / inputRes.xy;
				const float alpha = PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
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
	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwAlpha.IsValid())
	{
		const float2 alphaUV = (float2(coords) + 0.5f) / PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwAlpha.GetResolution().xy;
		const float outAlpha = dot(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inAlpha.Gather(BindlessSamplers::LinearClampEdge(), alphaUV), 0.25f);
		PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwAlpha.Store(coords, outAlpha);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwBaseColor.IsValid())
	{
		DownsampleSRGB(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inBaseColor, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwBaseColor, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwMetallicRoughness.IsValid())
	{
		DownsampleLinear(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inMetallicRoughness, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwMetallicRoughness, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwNormals.IsValid())
	{
		DownsampleNormals(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inNormals, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwNormals, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwEmissive.IsValid())
	{
		DownsampleSRGB(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inEmissive, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwEmissive, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwDepth.IsValid())
	{
		DownsampleLinear(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inDepth, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwDepth, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwOcclusion.IsValid())
	{
		DownsampleLinear(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inOcclusion, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwOcclusion, coords);
	}

	if (PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwDisplacement.IsValid())
	{
		DownsampleLinear(PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->inDisplacement, PARAMS_GENERATE_P_B_R_TEXTURES_MIPS_CONSTANTS->rwDisplacement, coords);
	}
}
