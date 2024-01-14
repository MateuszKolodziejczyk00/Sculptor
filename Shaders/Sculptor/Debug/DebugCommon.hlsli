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
