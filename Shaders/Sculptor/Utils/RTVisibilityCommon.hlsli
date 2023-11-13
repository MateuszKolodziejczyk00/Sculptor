[[descriptor_set(RTVisibilityDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]


struct RTVisibilityPayload
{
    bool isVisible;
};


[shader("miss")]
void RTVIsibilityRTM(inout RTVisibilityPayload payload)
{
    payload.isVisible = true;
}
