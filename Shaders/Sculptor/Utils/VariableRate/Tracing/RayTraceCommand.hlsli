#ifndef RAY_TRACE_COMMAND_HLSLI
#define RAY_TRACE_COMMAND_HLSLI


[[shader_struct(EncodedRayTraceCommand)]]


struct RayTraceCommand
{
	uint2 blockCoords;
	uint2 localOffset;
	uint variableRateMask;
};


EncodedRayTraceCommand EncodeTraceCommand(RayTraceCommand command)
{
	EncodedRayTraceCommand traceCommand;

	const uint coordX       = (command.blockCoords.x & 0xfff);
	const uint coordY       = (command.blockCoords.y & 0xfff) << 12;
	const uint localOffsetX = (command.localOffset.x & 0x3) << 24;
	const uint localOffsetY = (command.localOffset.y & 0x3) << 26;
	const uint rate         = (command.variableRateMask & 0xf) << 28;

	traceCommand.coordsAndRate = coordX | coordY | localOffsetX | localOffsetY | rate;

	return traceCommand;
}


RayTraceCommand DecodeTraceCommand(EncodedRayTraceCommand command)
{
	RayTraceCommand traceCommand;

	traceCommand.blockCoords.x = command.coordsAndRate & 0xfff;
	traceCommand.blockCoords.y = (command.coordsAndRate >> 12) & 0xfff;
	traceCommand.localOffset.x = (command.coordsAndRate >> 24) & 0x3;
	traceCommand.localOffset.y = (command.coordsAndRate >> 26) & 0x3;
	traceCommand.variableRateMask = (command.coordsAndRate >> 28) & 0xf;

	return traceCommand;
}

#endif // RAY_TRACE_COMMAND_HLSLI
