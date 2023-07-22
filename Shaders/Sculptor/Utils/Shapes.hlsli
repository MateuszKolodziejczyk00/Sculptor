#ifndef SHAPES_HLSLI
#define SHAPES_HLSLI

struct Sphere
{
    float3 center;
    float radius;
};


struct Ray
{
    float3 origin;
    float3 direction;
};


// Returns the distance along the ray where the intersection occurs.
// Based on: chapter 22.6.2 of "Real-Time Rendering, 3rd Edition"
float RaySphereIntersect(in Ray ray, in Sphere sphere)
{
    const float3 l = sphere.center - ray.origin;
    const float s = dot(l, ray.direction);
    const float l2 = dot(l, l);
    const float r2 = Pow2(sphere.radius);
    if(s < 0.f && l2 > r2)
    {
        return -1.f;
    }

    const float m2 = l2 - Pow2(s);

    if(m2 > r2)
    {
        return -1.f;
    }

    const float q = sqrt(r2 - m2);
    const float t = (l2 > r2) ? s - q : s + q;

    return t;
}


#endif // SHAPES_HLSLI