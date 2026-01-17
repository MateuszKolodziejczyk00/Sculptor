#ifndef COLOR_SPACES_HLSLI
#define COLOR_SPACES_HLSLI

float3 RGBToYCoCg(in float3 rgb)
{
	const float Y = rgb.r * 0.25f + rgb.g * 0.5f + rgb.b * 0.25f;
	const float Co = rgb.r * 0.5f + rgb.b * -0.5f;
	const float Cg = rgb.r * -0.25f + rgb.g * 0.5f + rgb.b * -0.25f;
	return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(in float3 ycocg)
{
	const float r = ycocg.x + ycocg.y - ycocg.z;
	const float g = ycocg.x + ycocg.z;
	const float b = ycocg.x - ycocg.y - ycocg.z;
	return float3(r, g, b);
}

float3 RGBToXYZ( float3 x )
{
	static const float3x3 m =
	{
		0.4123907992659595, 0.3575843393838780, 0.1804807884018343,
		0.2126390058715104, 0.7151686787677559, 0.0721923153607337,
		0.0193308187155918, 0.1191947797946259, 0.9505321522496608
	};

	return mul(m, x);
}

float3 XYZToRGB( float3 x )
{
	static const float3x3 m =
	{
		3.240969941904522, -1.537383177570094, -0.4986107602930032,
		-0.9692436362808803, 1.875967501507721, 0.04155505740717569,
		0.05563007969699373, -0.2039769588889765, 1.056971514242878
	};

	return mul(m, x);
}

// Source: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab
float3 LinearTosRGB(in float3 color)
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}

#endif // COLOR_SPACES_HLSLI
