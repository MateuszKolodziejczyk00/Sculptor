#pragma once

#include "SculptorCoreTypes.h"
#include "TinyGLTF.h"


namespace spt::rsc
{

using GLTFModel = tinygltf::Model;


std::optional<GLTFModel> LoadGLTFModel(const lib::String& path);

} // spt::rsc