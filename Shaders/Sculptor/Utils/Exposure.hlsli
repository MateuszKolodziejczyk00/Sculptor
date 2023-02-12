// Source https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/course-notes-moving-frostbite-to-pbr-v2.pdf page 85

float ComputeEV100FromAvgLuminance(float avgLuminance)
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference : http://en.wikipedia.org/wiki/Film_speed
    return log2(avgLuminance * 100.f / 12.5f);
}

float ConvertEV100ToMaxLuminance(float EV100)
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = 78 / ( S * q ) * N^2 / t
    // = 78 / ( S * q ) * 2^ EV_100
    // = 78 / (100 * 0.65) * 2^ EV_100
    // = 1.2 * 2^ EV
    // Reference : http://en.wikipedia.org/wiki/Film_speed
    return 1.2f * exp2(EV100);
}

float ConvertEV100ToExposure(float EV100)
{
    return 1.f / ConvertEV100ToMaxLuminance(EV100);
}
