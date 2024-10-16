#include "SculptorShader.hlsli"

[[descriptor_set(RTCopyTracedReservoirsDS, 0)]]

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(32, 1, 1)]
void RTCopyTracedReservoirsCS(CS_INPUT input)
{
	const uint traceCommandIndex = input.globalID.x;

	if(traceCommandIndex >= u_tracesNum[0])
	{
		return;
	}

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const uint reservoirIdx = GetScreenReservoirIdx(coords, u_resamplingConstants.reservoirsResolution);

	u_outReservoirs[reservoirIdx] = u_inReservoirs[reservoirIdx];
}