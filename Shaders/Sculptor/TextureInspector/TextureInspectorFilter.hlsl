#include "SculptorShader.hlsli"

[[descriptor_set(TextureInspectorFilterDS, 0)]]


#define VISUALIZATION_MODE_COLOR 0
#define VISUALIZATION_MODE_ALPHA 1
#define VISUALIZATION_MODE_HASH  2


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


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
			textureValue = u_intTexture.Load(pixel);

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
			textureValue = u_floatTexture.Load(pixel);
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

		color.rgb = pow(color.rgb, 0.4545f);

		color.a = 1.f;

		u_outputTexture[pixel.xy] = color;
	}
}
