#ifndef DEBUG_PRINTF_HLSLI
#define DEBUG_PRINTF_HLSLI

#if SPT_SHADERS_DEBUG_FEATURES

#include "Debug/DebugCommandsWriter.hlsli"

#define PRINTF_OP_CODE uint(1)

#define SPT_DEBUG_PRINTF(literal, ...) \
{ \
debug::DebugCommandsWriter writer = debug::CreateDebugCommandsWriter(); \
writer.Write(PRINTF_OP_CODE); \
writer.WriteLiteral(literal); \
writer.WriteWithCodes(__VA_ARGS__); \
writer.Flush(); \
}

#endif // SPT_SHADERS_DEBUG_FEATURES

#endif // DEBUG_PRINTF_HLSLI