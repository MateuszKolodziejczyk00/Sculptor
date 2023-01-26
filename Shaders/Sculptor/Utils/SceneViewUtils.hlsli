
float GetNearPlane(float4x4 projectionMatrix)
{
    return projectionMatrix[3][0];
}