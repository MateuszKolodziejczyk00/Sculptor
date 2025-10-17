#pragma once

#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "RHICore/RHIBufferTypes.h"
#include "Material.h"


namespace spt::mat
{

struct MaterialDefinition
{
	MaterialDefinition()
		: customOpacity(false)
		, doubleSided(false)
		, emissive(false)
		, transparent(false)
	{ }

	lib::HashedString name;

	Uint8 customOpacity : 1;
	Uint8 doubleSided   : 1;
	Uint8 emissive      : 1;
	Uint8 transparent   : 1;
};


struct MaterialDataParameters
{
	rhi::RHIVirtualAllocation suballocation;
	lib::HashedString         materialDataStructName;
};


class IMaterialFactory
{
public:

	virtual ~IMaterialFactory() = default;

	virtual ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle materialShaderHandle, const MaterialDataParameters& materialData) = 0;
};


class MaterialFactory : public IMaterialFactory
{
public:

	virtual ecs::EntityHandle CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle materialShaderHandle, const MaterialDataParameters& materialData) override;
};


} // spt::mat