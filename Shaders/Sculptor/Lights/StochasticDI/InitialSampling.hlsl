#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(StochasticDIInitialSamplingDS)]]

#include "RayTracing/RayTracingHelpers.hlsli"


[shader("raygeneration")]
void InitialSamplingRTG()
{

}
