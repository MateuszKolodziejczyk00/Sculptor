#ifndef DEBUG_COMMON_HLSLI
#define DEBUG_COMMON_HLSLI

namespace debug
{

class Literal
{
	uint2 val;
};

Literal CreateLiteral(uint2 val)
{
	Literal result;
	result.val = val;
	return result;
}

} // debug

#if SPT_META_PARAM_DEBUG_FEATURES


#define SPT_LITERAL_INTERNAL(literal) L##literal
#define SPT_LITERAL(literal) SPT_LITERAL_INTERNAL(literal)

#define SPT_FILE SPT_LITERAL(__FILE__)

#define SPT_LINE __LINE__


[[descriptor_set(ShaderDebugCommandBufferDS, 15)]]


namespace debug
{

bool IsPixelHovered(uint2 coords, uint2 resolution)
{
	return all(round(u_debugCommandsBufferParams.mouseUV * resolution) == coords);
}

void WriteDebugPixel(uint2 pixel, float4 value)
{
	u_debugOutputTexture[pixel] = value;
}

void WriteDebugPixel(uint2 pixel, float3 value)
{
	WriteDebugPixel(pixel, float4(value, 0.f));
}

void WriteDebugPixel(uint2 pixel, float2 value)
{
	WriteDebugPixel(pixel, float4(value, 0.f, 0.f));
}

void WriteDebugPixel(uint2 pixel, float value)
{
	WriteDebugPixel(pixel, float4(value, 0.f, 0.f, 0.f));
}

} // debug

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // DEBUG_COMMON_HLSLI
