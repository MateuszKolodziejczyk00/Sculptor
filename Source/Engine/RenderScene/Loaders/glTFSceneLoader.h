#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;

namespace glTFLoader
{

void RENDER_SCENE_API LoadScene(RenderScene& scene, lib::StringView path);

} // glTFLoader

} // spt::rsc