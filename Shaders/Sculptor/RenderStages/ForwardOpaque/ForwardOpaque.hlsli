
struct FO_PS_OUTPUT
{
    float4 luminance                 : SV_TARGET0;
    float4 eyeAdaptationLuminance    : SV_TARGET0;
    float4 normal                    : SV_TARGET1;
    
#if WITH_DEBUGS
    float4 debug : SV_TARGET2;
#endif // WITH_DEBUGS
};