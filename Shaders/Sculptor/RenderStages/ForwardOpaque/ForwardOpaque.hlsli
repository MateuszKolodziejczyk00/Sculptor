
struct FO_PS_OUTPUT
{
    float4 radiance : SV_TARGET0;
    float4 normal   : SV_TARGET1;
    
#if WITH_DEBUGS
    float4 debug    : SV_TARGET2;
#endif // WITH_DEBUGS
};