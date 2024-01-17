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

#if SPT_SHADERS_DEBUG_FEATURES


#define SPT_LITERAL_INTERNAL(literal) L##literal
#define SPT_LITERAL(literal) SPT_LITERAL_INTERNAL(literal)

#define SPT_FILE SPT_LITERAL(__FILE__)

#define SPT_LINE __LINE__


[[descriptor_set(ShaderDebugCommandBufferDS, 15)]]


namespace debug
{

bool IsPixelHovered(uint2 pixelPos)
{
    return all(int2(pixelPos) == u_debugCommandsBufferParams.mousePosition);
}

bool IsPixelHoveredHalfRes(uint2 pixelPos)
{
    return all(int2(pixelPos) == u_debugCommandsBufferParams.mousePositionHalfRes);
}

} // debug

#endif // SPT_SHADERS_DEBUG_FEATURES

#endif // DEBUG_COMMON_HLSLI
