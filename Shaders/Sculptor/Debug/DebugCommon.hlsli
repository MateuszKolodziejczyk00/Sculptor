#ifndef DEBUG_COMMON_HLSLI
#define DEBUG_COMMON_HLSLI

namespace debug
{

struct Literal
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


[[shader_params(ShaderDebugCommandBufferParams, PARAMS_SHADER_DEBUG_COMMAND_BUFFER)]]


namespace debug
{

bool HasValidCursorPos()
{
	return all(PARAMS_SHADER_DEBUG_COMMAND_BUFFER->mouseUV > 0.f);
}

bool IsPixelHovered(uint2 coords, uint2 resolution)
{
	return all(round(PARAMS_SHADER_DEBUG_COMMAND_BUFFER->mouseUV * resolution) == coords);
}

void WriteDebugPixel(uint2 pixel, float4 value)
{
	PARAMS_SHADER_DEBUG_COMMAND_BUFFER->debugOutputTexture[pixel] = value;
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

void WriteDebugPixelOnScreen(uint2 pixel, float4 value)
{
	PARAMS_SHADER_DEBUG_COMMAND_BUFFER->debugOnScreenOutputTexture[pixel] = value;
}

} // debug

#endif // SPT_META_PARAM_DEBUG_FEATURES

#endif // DEBUG_COMMON_HLSLI
