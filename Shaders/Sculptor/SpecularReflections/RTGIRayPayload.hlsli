#ifndef RTGI_RAY_PAYLOAD_HLSLI
#define RTGI_RAY_PAYLOAD_HLSLI

struct RTGIRayPayload
{
    half3 normal;
    half roughness;
    half3 emissive;
    uint baseColorMetallic;
    float distance;
};

#endif // RTGI_RAY_PAYLOAD_HLSLI
