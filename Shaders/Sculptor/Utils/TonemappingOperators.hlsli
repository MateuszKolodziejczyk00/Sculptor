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


float3 InverseTonemapACES(float3 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (-0.59 * x + 0.03 - sqrt(-1.0127 * x*x + 1.3702 * x + 0.0009)) / (2.0 * (2.43*x - 2.51));
}