#ifndef RTREFLECTIONSSHADINGCOMMON_HLSLI
#define RTREFLECTIONSSHADINGCOMMON_HLSLI

#include "SpecularReflections/SRReservoir.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


void WriteReservoirToScreenBuffer(in RWStructuredBuffer<SRPackedReservoir> reservoirsBuffer, in uint2 reservoirsRes, in SRReservoir reservoir, in RayTraceCommand traceCommand)
{
	const SRPackedReservoir packedReservoir = PackReservoir(reservoir);

	const uint2 traceCoords = traceCommand.blockCoords + traceCommand.localOffset;

	const uint reservoirIdx = GetScreenReservoirIdx(traceCoords, reservoirsRes);
	reservoirsBuffer[reservoirIdx] = packedReservoir;
}


struct GeneratedRayPDF
{
	float pdf;
	bool isSpecularTrace;
};


GeneratedRayPDF LoadGeneratedRayPDF(in StructuredBuffer<float> rayPdfs, in uint rayIdx)
{
	const float pdf = rayPdfs[rayIdx];
	const bool isSpecular = pdf < 0.f;

	GeneratedRayPDF rayPdf;
	rayPdf.pdf             = isSpecular ? 1.f : pdf;
	rayPdf.isSpecularTrace = isSpecular;

	return rayPdf;
}

#endif // RTREFLECTIONSSHADINGCOMMON_HLSLI