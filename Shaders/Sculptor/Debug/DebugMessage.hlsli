#ifndef DEBUG_MESSAGE_HLSLI
#define DEBUG_MESSAGE_HLSLI


#if SPT_SHADERS_DEBUG_FEATURES

#include "Debug/DebugCommandsWriter.hlsli"

#define PRINTF_OP_CODE uint(1)
#define ASSERTION_FAILED_MESSAGE_OP_CODE uint(2)

#define SPT_DEBUG_PRINTF(literal, ...) \
{ \
debug::DebugCommandsWriter writer = debug::CreateDebugCommandsWriter(); \
writer.Write(PRINTF_OP_CODE); \
writer.WriteLiteral(literal); \
writer.WriteWithCodes(__VA_ARGS__); \
writer.Flush(); \
}

#define SPT_CHECK_MSG(condition, message, ...) \
{ \
if(!(condition)) \
{ \
debug::DebugCommandsWriter writer = debug::CreateDebugCommandsWriter(); \
writer.Write(ASSERTION_FAILED_MESSAGE_OP_CODE); \
writer.WriteLiteral(SPT_FILE); \
writer.Write(uint(SPT_LINE)); \
writer.WriteLiteral(SPT_LITERAL(#condition)); \
writer.WriteLiteral(message); \
writer.WriteWithCodes(__VA_ARGS__); \
writer.Flush(); \
} \
}

#endif // SPT_SHADERS_DEBUG_FEATURES

#endif // DEBUG_MESSAGE_HLSLI