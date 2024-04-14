#include "SculptorShader.hlsli"

[[descriptor_set(TextureInspectorFilterDS, 0)]]


#define VISUALIZATION_MODE_COLOR 0
#define VISUALIZATION_MODE_ALPHA 1
#define VISUALIZATION_MODE_NANS  2
#define VISUALIZATION_MODE_HASH  3


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define HISTOGRAM_BINS_NUM 128


void OutputHistogram(in float4 textureValue, in float minValue, in float maxValue)
{
	const float4 histogramValue = float4(textureValue.rgb, dot(textureValue.rgb, 0.3333f));
	const uint4 bins = uint4(clamp((histogramValue - minValue) / (maxValue - minValue) * HISTOGRAM_BINS_NUM, 0.f, HISTOGRAM_BINS_NUM - 1.f));
	
	[unroll]
	for(uint i = 0u; i < 4u; ++i)
	{
		InterlockedAdd(u_histogram[bins[i]][i], 1u);
	}
}


[numthreads(8, 8, 1)]
void TextureInspectorFilterCS(CS_INPUT input)
{
	const uint3 pixel = input.globalID;
	
	uint2 outputRes;
	u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		float4 textureValue = 0.f;

		float4 color = 0.f;
		if(u_params.isIntTexture)
		{
			if(u_params.depthSlice3D != IDX_NONE_32)
			{
				textureValue = u_intTexture.Load(pixel);
			}
			else
			{
				textureValue = u_intTexture3D.Load(int4(pixel.xy, u_params.depthSlice3D, 0));
			}

			if (u_params.visualizationMode == VISUALIZATION_MODE_HASH)
			{
				const uint intValue = textureValue.x;
				const uint valueHash = HashPCG(intValue);
				color.rgb = float3((valueHash >> 16) & 0xFF, (valueHash >> 8) & 0xFF, valueHash & 0xFF) / 255.f;
			}
			else
			{
				color = textureValue;
			}
		}
		else
		{
			if(u_params.depthSlice3D != IDX_NONE_32)
			{
				textureValue = u_floatTexture3D.Load(int4(pixel.xy, u_params.depthSlice3D, 0));
			}
			else
			{
				textureValue = u_floatTexture.Load(pixel);
			}

			color = textureValue;
		}

		if (all(pixel.xy == u_params.hoveredPixel))
		{
			u_readbackBuffer[0].hoveredPixelValue = textureValue;
		}

		if (u_params.visualizationMode == VISUALIZATION_MODE_COLOR)
		{
			if (!u_params.rChannelVisible)
			{
				color.r = 0.f;
			}
			if (!u_params.gChannelVisible)
			{
				color.g = 0.f;
			}
			if (!u_params.bChannelVisible)
			{
				color.b = 0.f;
			}

			color.rgb = clamp(color.rgb, u_params.minValue, u_params.maxValue);

			color.rgb = (color.rgb - u_params.minValue) / (u_params.maxValue - u_params.minValue);
		}
		else if(u_params.visualizationMode == VISUALIZATION_MODE_ALPHA)
		{
			color.rgb = color.aaa;
		}

		if (u_params.visualizationMode == VISUALIZATION_MODE_NANS)
		{
			if(any(isnan(textureValue)))
			{
				color.rgb = float3(1.f, 0.f, 0.f);
			}
			else
			{
				color.rgb = float3(0.f, 0.f, 0.f);
			}
		}

		if (u_params.shouldOutputHistogram)
		{
			OutputHistogram(textureValue, u_params.minValue, u_params.maxValue);
		}

		color.rgb = pow(color.rgb, 0.4545f);

		color.a = 1.f;

		u_outputTexture[pixel.xy] = color;
	}
}
