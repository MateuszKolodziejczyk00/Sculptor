#pragma once

#include "SculptorCoreTypes.h"
#include "AssetTypes.h"


namespace spt::as
{

struct PBRMaterialDefinition;


lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRMaterialDefinition& definition);

} // spt::as
