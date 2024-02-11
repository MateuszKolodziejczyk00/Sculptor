#include "SculptorShader.hlsli"

[[descriptor_set(TextureInspectorFilterDS, 0)]]


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
		float3 color = 0.f;
		if(u_params.isIntTexture)
		{
			color = u_intTexture.Load(pixel).rgb;
		}
		else
		{
			color = u_floatTexture.Load(pixel).rgb;
		}

		if (all(pixel.xy == u_params.hoveredPixel))
		{
			u_readbackBuffer[0].hoveredPixelValue = float4(color, 1.f);
		}


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

		color = clamp(color, u_params.minValue, u_params.maxValue);

		color = (color - u_params.minValue) / (u_params.maxValue - u_params.minValue);

		color = pow(color, 0.4545f);

		u_outputTexture[pixel.xy] = float4(color, 1.f);
	}
}
