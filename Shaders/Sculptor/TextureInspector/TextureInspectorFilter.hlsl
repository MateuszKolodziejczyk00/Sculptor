#include "SculptorShader.hlsli"

[[shader_params(TextureInspectorFilterConstants, PARAMS_TEXTURE_INSPECTOR_FILTER)]]

#include "Utils/ColorSpaces.hlsli"


#define VISUALIZATION_MODE_COLOR 0
#define VISUALIZATION_MODE_ALPHA 1
#define VISUALIZATION_MODE_NANS  2
#define VISUALIZATION_MODE_HASH  3


#define COLOR_SPACE_LINEAR_RGB 0
#define COLOR_SPACE_SRGB       1


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define HISTOGRAM_BINS_NUM 128


void OutputHistogram(in float4 textureValue, in float minValue, in float maxValue)
{
	const float4 histogramValue = float4(textureValue.rgb, dot(textureValue.rgb, 0.3333f));
	const uint4 bins = uint4(clamp((histogramValue - minValue) / (maxValue - minValue) * HISTOGRAM_BINS_NUM, 0.f, HISTOGRAM_BINS_NUM - 1.f));
	
	//[unroll]
	//for(uint i = 0u; i < 4u; ++i)
	//{
	//	InterlockedAdd(PARAMS_TEXTURE_INSPECTOR_FILTER->histogram[bins[i]][i], 1u);
	//}
}


[numthreads(8, 8, 1)]
void TextureInspectorFilterCS(CS_INPUT input)
{
	const uint3 pixel = input.globalID;
	
	uint2 outputRes = PARAMS_TEXTURE_INSPECTOR_FILTER->outputTexture.GetResolution();

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		float4 textureValue = 0.f;

		float4 color = 0.f;
		if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.isIntTexture)
		{
			if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.depthSlice3D != IDX_NONE_32)
			{
				textureValue = PARAMS_TEXTURE_INSPECTOR_FILTER->intTexture3D.Load(int4(pixel.xy, PARAMS_TEXTURE_INSPECTOR_FILTER->params.depthSlice3D, 0));
			}
			else
			{
				textureValue = PARAMS_TEXTURE_INSPECTOR_FILTER->intTexture.Load(pixel);
			}

			if (PARAMS_TEXTURE_INSPECTOR_FILTER->params.visualizationMode == VISUALIZATION_MODE_HASH)
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
			if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.depthSlice3D != IDX_NONE_32)
			{
				textureValue = PARAMS_TEXTURE_INSPECTOR_FILTER->floatTexture3D.Load(int4(pixel.xy, PARAMS_TEXTURE_INSPECTOR_FILTER->params.depthSlice3D, 0));
			}
			else
			{
				textureValue = PARAMS_TEXTURE_INSPECTOR_FILTER->floatTexture.Load(pixel);
			}

			color = textureValue;
		}

		if (all(pixel.xy == PARAMS_TEXTURE_INSPECTOR_FILTER->params.hoveredPixel))
		{
			PARAMS_TEXTURE_INSPECTOR_FILTER->readbackBuffer[0].hoveredPixelValue = textureValue;
		}

		if (PARAMS_TEXTURE_INSPECTOR_FILTER->params.visualizationMode == VISUALIZATION_MODE_COLOR)
		{
			if (!PARAMS_TEXTURE_INSPECTOR_FILTER->params.rChannelVisible)
			{
				color.r = 0.f;
			}
			if (!PARAMS_TEXTURE_INSPECTOR_FILTER->params.gChannelVisible)
			{
				color.g = 0.f;
			}
			if (!PARAMS_TEXTURE_INSPECTOR_FILTER->params.bChannelVisible)
			{
				color.b = 0.f;
			}

			color.rgb = clamp(color.rgb, PARAMS_TEXTURE_INSPECTOR_FILTER->params.minValue, PARAMS_TEXTURE_INSPECTOR_FILTER->params.maxValue);

			color.rgb = (color.rgb - PARAMS_TEXTURE_INSPECTOR_FILTER->params.minValue) / (PARAMS_TEXTURE_INSPECTOR_FILTER->params.maxValue - PARAMS_TEXTURE_INSPECTOR_FILTER->params.minValue);
		}
		else if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.visualizationMode == VISUALIZATION_MODE_ALPHA)
		{
			color.rgb = color.aaa;
		}

		if (PARAMS_TEXTURE_INSPECTOR_FILTER->params.visualizationMode == VISUALIZATION_MODE_NANS)
		{
			if(any(isnan(textureValue)) || any(isinf(textureValue)))
			{
				color.rgb = float3(1.f, 0.f, 0.f);
			}
			else
			{
				color.rgb = float3(0.f, 0.f, 0.f);
			}
		}

		if (PARAMS_TEXTURE_INSPECTOR_FILTER->params.shouldOutputHistogram)
		{
			OutputHistogram(textureValue, PARAMS_TEXTURE_INSPECTOR_FILTER->params.minValue, PARAMS_TEXTURE_INSPECTOR_FILTER->params.maxValue);
		}

		if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.colorSpace == COLOR_SPACE_LINEAR_RGB)
		{
			color.rgb = LinearTosRGB(color.rgb);
		}
		else if(PARAMS_TEXTURE_INSPECTOR_FILTER->params.colorSpace == COLOR_SPACE_SRGB)
		{
			// Do nothing
		}

		color.a = 1.f;

		PARAMS_TEXTURE_INSPECTOR_FILTER->outputTexture[pixel.xy] = color;
	}
}
