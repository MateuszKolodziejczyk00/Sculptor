float3 TonemapACES(float3 x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}


float3 InverseTonemapACES(float3 x)
    {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (-0.59 * x + 0.03 - sqrt(-1.0127 * x*x + 1.3702 * x + 0.0009)) / (2.0 * (2.43*x - 2.51));
}


// ACES Fitted curve
// Source: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// by MJP and David Neubelt
// http://mynameismjp.wordpress.com/
// The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
// credit for coming up with this fit and implementing it. Buy him a beer next time you see him. :)


// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};


// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};


float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}


float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

// Grand Turismo Tonemapper
// Based on: https://github.com/yaoling1997/GT-ToneMapping

float W_f(float x,float e0,float e1) {
	if (x <= e0)
		return 0;
	if (x >= e1)
		return 1;
	float a = (x - e0) / (e1 - e0);
	return a * a*(3 - 2 * a);
}
float H_f(float x, float e0, float e1) {
	if (x <= e0)
		return 0;
	if (x >= e1)
		return 1;
	return (x - e0) / (e1 - e0);
}

float GTTonemapper(float x) {
	float P = 1;
	float a = 1;
	float m = 0.22;
	float l = 0.4;
	float c = 1.33;
	float b = 0;
	float l0 = (P - m)*l / a;
	float L0 = m - m / a;
	float L1 = m + (1 - m) / a;
	float L_x = m + a * (x - m);
	float T_x = m * pow(x / m, c) + b;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = a * P / (P - S1);
	float S_x = P - (P - S1)*pow(2.71828,-(C2*(x-S0)/P));
	float w0_x = 1 - W_f(x, 0, m);
	float w2_x = H_f(x, m + l0, m + l0);
	float w1_x = 1 - w0_x - w2_x;
	float f_x = T_x * w0_x + L_x * w1_x + S_x * w2_x;
	return f_x;
}