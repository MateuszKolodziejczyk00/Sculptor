#ifndef RTREFLECTIONSSHADINGCOMMON_HLSLI
#define RTREFLECTIONSSHADINGCOMMON_HLSLI

#include "SpecularReflections/SRReservoir.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


void WriteReservoirToScreenBuffer(in RWStructuredBuffer<SRPackedReservoir> reservoirsBuffer, in uint2 reservoirsRes, in SRReservoir reservoir, in RayTraceCommand traceCommand)
{
	const SRPackedReservoir packedReservoir = PackReservoir(reservoir);

	const uint2 blockCoords = traceCommand.blockCoords;
	const uint2 blockSize = GetVariableRateTileSize(traceCommand.variableRateMask);
	const uint2 traceCoords = blockCoords + traceCommand.localOffset;

	const uint reservoirIdx = GetScreenReservoirIdx(traceCoords, reservoirsRes);
	reservoirsBuffer[reservoirIdx] = packedReservoir;
}

#endif // RTREFLECTIONSSHADINGCOMMON_HLSLI