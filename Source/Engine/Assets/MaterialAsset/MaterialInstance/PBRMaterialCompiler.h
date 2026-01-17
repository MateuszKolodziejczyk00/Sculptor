#pragma once

#include "SculptorCoreTypes.h"
#include "AssetTypes.h"


namespace spt::as
{

struct PBRMaterialDefinition;
struct PBRGLTFMaterialDefinition;


namespace material_compiler
{

lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRMaterialDefinition& definition);
lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRGLTFMaterialDefinition& definition);

} // material_compiler

} // spt::as
