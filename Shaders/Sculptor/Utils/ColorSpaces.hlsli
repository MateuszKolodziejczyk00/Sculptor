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

float3 SRGBToLinear(in float3 color)
{
	float3 x = color / 12.92f;
	float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

	float3 clr = color;
	clr.r = color.r <= 0.04045f ? x.r : y.r;
	clr.g = color.g <= 0.04045f ? x.g : y.g;
	clr.b = color.b <= 0.04045f ? x.b : y.b;

	return clr;
}


float4 RGBToLMSR(float3 c)
{
	const float4x3 m = float4x3(
		7.69684945, 18.4248204, 2.06809497,
		2.43113687, 18.6979422, 3.01246326,
		0.28911757, 1.40183293, 13.7922962,
		0.46638595, 15.5643680, 10.0599647
		);

	return mul(m, c);
}


// Inverse of the LMS part of the LMSR conversion
float3 LMSR3ToRGB(float3 c)
{
	const float3x3 m = float3x3(
		0.18838368, -0.18656929, 0.01250247,
		-0.02425491, 0.07839348, -0.01348550,
		-0.00148371, -0.00405691, 0.07361281
	);

	return mul(m, c);
}


// https://en.wikipedia.org/wiki/LMS_color_space#XYZ_to_LMS
float3 XYZToLMSBradford(float3 c)
{
	const float3x3 m = float3x3(
		0.8951f, 0.2664f, -0.1614f,
		-0.7502f, 1.7135f, 0.0367f,
		0.0389f, -0.0685f, 1.0296f);

	return mul(m, c);
}


float3 LMSToXYZBradford(float3 c)
{
	const float3x3 m = float3x3(
		0.9869929f, -0.1470543f, 0.1599627f,
		0.4323053f, 0.5183603f, 0.0492912f,
		-0.0085287f, 0.0400428f, 0.9684867f);

	return mul(m, c);
}


// Based on:
// https://www.scratchapixel.com/lessons/cg-gems/blackbody/blackbody.html
float3 ComputePlanckLocus(float temperature)
{
	float x_c, y_c;
	const float arg1 = 1e9 / Pow3(temperature);
	const float arg2 = 1e6 / Pow2(temperature);
	const float arg3 = 1e3 / temperature;

	if (temperature >= 1667 && temperature <= 4000)
	{
		x_c = -0.2661239 * arg1 - 0.2343580 * arg2 + 0.8776956 * arg3 + 0.179910;
	}
	else if (temperature > 4000 && temperature <= 25000)
	{
		x_c = -3.0258469 * arg1 + 2.1070379 * arg2 + 0.2226347 * arg3 + 0.240390;
	}

	float x_c3 = x_c * x_c * x_c, x_c2 = x_c * x_c;

	if (temperature >= 1667 && temperature <= 2222)
	{
		y_c = -1.1063814 * x_c3 - 1.34811020 * x_c2 + 2.18555832 * x_c - 0.20219683;
	}
	else if (temperature > 2222 && temperature <= 4000)
	{
		y_c = -0.9549476 * x_c3 - 1.37418593 * x_c2 + 2.09137015 * x_c - 0.16748867;
	}
	else if (temperature > 4000 && temperature <= 25000)
	{
		y_c = +3.0817580 * x_c3 - 5.87338670 * x_c2 + 3.75112997 * x_c - 0.37001483;
	}

	float X = x_c / y_c;
	float Y = 1.f;
	float Z = (1.f - x_c - y_c) / y_c;

	return float3(X, Y, Z);
}


float3 Rec709ToRec2020(in float3 c)
{
	const float3x3 m = float3x3(
		0.6274040f, 0.3292820f, 0.0433136f,
		0.0690970f, 0.9195400f, 0.0113612f,
		0.0163916f, 0.0880132f, 0.8955952f);

	return mul(m, c);
}


float3 Rec2020ToRec709(in float3 c)
{
	const float3x3 m = float3x3(
		1.6604910f, -0.5876410f, -0.0728508f,
		-0.1245508f, 1.1328950f, -0.0083442f,
		-0.0181507f, -0.1005794f, 1.1187298f);

	return mul(m, c);
}

#endif // COLOR_SPACES_HLSLI
