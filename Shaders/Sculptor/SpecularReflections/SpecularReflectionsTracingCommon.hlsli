[[descriptor_set(SpecularReflectionsTraceDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshUnifiedDataDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[descriptor_set(RenderViewDS, 5)]]


struct SpecularReflectionsRayPayload
{
    half3 normal;
    half roughness;
    half3 emissive;
    uint baseColorMetallic;
    float distance;
};
