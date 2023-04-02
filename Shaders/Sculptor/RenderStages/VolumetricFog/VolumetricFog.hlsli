
float4 ComputeScatteringAndExtinction(in float3 fogColor, in float fogDensity, in float scatteringFactor)
{
    const float extinction = scatteringFactor * fogDensity;
    return float4(fogColor * extinction, extinction);
}


float ComputeFogFroxelLinearDepth(in float depth, in float fogNearPlane, in float fogFarPlane)
{
    return fogNearPlane * pow(fogFarPlane / fogNearPlane, depth);
}


float3 FogFroxelToNDC(in float2 fogFroxelUV, in float fogFroxelLinearDepth, in float projectionNear)
{
    return float3(fogFroxelUV * 2.f - 1.f, projectionNear / fogFroxelLinearDepth);
}


float3 ComputeFogFroxelUVW(in float2 uv, in float linearDepth, in float fogNearPlane, in float fogFarPlane)
{
    const float w = log(linearDepth / fogNearPlane) / log(fogFarPlane / fogNearPlane);
    return float3(uv, w);
}
