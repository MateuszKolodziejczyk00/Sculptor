#ifndef RTGI_TRACING_COMMON_HLSLI
#define RTGI_TRACING_COMMON_HLSLI

[[descriptor_set(SceneRayTracingDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshUnifiedDataDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[descriptor_set(RenderViewDS, 5)]]

#include "SpecularReflections/RTGIRayPayload.hlsli"

#endif // RTGI_TRACING_COMMON_HLSLI
