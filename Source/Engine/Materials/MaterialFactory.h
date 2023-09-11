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
		: materialType(EMaterialType::Invalid)
		, customOpacity(false)
	{ }

	lib::HashedString	name;
	EMaterialType		materialType;

	Uint8 customOpacity : 1;
};


struct MaterialDataParameters
{
	rhi::RHISuballocation	suballocation;
	lib::HashedString		materialDataStructName;
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