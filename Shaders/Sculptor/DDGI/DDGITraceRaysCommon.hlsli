
[[descriptor_set(DDGITraceRaysDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshUnifiedDataDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[descriptor_set(DDGILightsDS, 5)]]

[[descriptor_set(ShadowMapsDS, 6)]]


struct DDGIRayPayload
{
    half3 normal;
    half roughness;
    uint baseColorMetallic;
    float hitDistance;
};