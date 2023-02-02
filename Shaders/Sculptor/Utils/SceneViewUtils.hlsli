
float GetNearPlane(float4x4 projectionMatrix)
{
    return projectionMatrix[3][0];
}


// Based on: 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool GetProjectedSphereAABB(float3 viewSpaceCenter, float radius, float znear, float P01, float P12, out float4 aabb)
{
    if (viewSpaceCenter.x < radius + znear)
    {
        return false;
    }

    float3 cr = viewSpaceCenter * radius;
    float czr2 = viewSpaceCenter.x * viewSpaceCenter.x - radius * radius;

    float vx = sqrt(viewSpaceCenter.y * viewSpaceCenter.y + czr2);
    float minx = (vx * viewSpaceCenter.y - cr.x) / (vx * viewSpaceCenter.x + cr.y);
    float maxx = (vx * viewSpaceCenter.y + cr.x) / (vx * viewSpaceCenter.x - cr.y);

    float vy = sqrt(viewSpaceCenter.z * viewSpaceCenter.z + czr2);
    float miny = (vy * viewSpaceCenter.z - cr.x) / (vy * viewSpaceCenter.x + cr.z);
    float maxy = (vy * viewSpaceCenter.z + cr.x) / (vy * viewSpaceCenter.x - cr.z);

    aabb = float4(minx * P01, miny * P12, maxx * P01, maxy * P12);
    // clip space -> uv space
    aabb = aabb * 0.5f + 0.5f;

    return true;
}
