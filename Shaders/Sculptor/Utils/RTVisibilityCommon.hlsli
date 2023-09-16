[[descriptor_set(RenderSceneDS, 0)]]

[[descriptor_set(RTVisibilityDS, 1)]]


struct RTVisibilityPayload
{
    bool isVisible;
};


[shader("miss")]
void RTVIsibilityRTM(inout RTVisibilityPayload payload)
{
    payload.isVisible = true;
}
