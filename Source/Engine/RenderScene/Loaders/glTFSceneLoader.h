#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;

namespace glTFLoader
{

void LoadScene(RenderScene& scene, lib::StringView path);

} // glTFLoader

} // spt::rsc