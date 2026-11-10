#pragma once

#include "Entities/PrefabEntity.h"
#include "GamePrefab.h"
#include "Lights/LightingScene.h"
#include "IESProfileAsset.h"


namespace spt::gf
{

struct PrefabLightEntityDefinition : public GamePrefabEntityDefinition
{
	rsc::ELocalLightType type = rsc::ELocalLightType::PointLight;

	math::Vector3f color = math::Vector3f::Ones();

	// Luminous power in lumens
	Real32 luminousPower = 1.f;
	Real32 range         = 1.f;

	// Spot only, in degrees
	Real32 innerHalfAngle = 22.5f;
	Real32 outerHalfAngle = 45.f;

	Bool contributeToDI         = true;
	Bool contributeToScattering = true;

	lib::Path iesProfile;

	virtual lib::RuntimeTypeInfo    GetType() const override { return lib::TypeInfo<PrefabLightEntityDefinition>(); }
	virtual lib::DynamicArray<Byte> Compile(as::PrefabCompiler& compiler) const override;

	static void Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData);

	virtual void Serialize(srl::Serializer& serializer) override
	{
		PrefabEntityDefinition::Serialize(serializer);

		serializer.Serialize("Type",                   type);
		serializer.Serialize("Color",                  color);
		serializer.Serialize("LuminousPower",          luminousPower);
		serializer.Serialize("Range",                  range);
		serializer.Serialize("InnerHalfAngle",         innerHalfAngle);
		serializer.Serialize("OuterHalfAngle",         outerHalfAngle);
		serializer.Serialize("ContributeToDI",         contributeToDI);
		serializer.Serialize("ContributeToScattering", contributeToScattering);
		serializer.Serialize("IESProfile",             iesProfile);
	}
};
SPT_REGISTER_PREFAB_ENTITY_TYPE(PrefabLightEntityDefinition);


using LightEntityHandle = lib::PagedGenerationalPoolHandle<struct LightEntity>;


struct LightEntity
{
	struct Def
	{
		Transform                 transform;
		rsc::ELocalLightType      type           = rsc::ELocalLightType::PointLight;
		math::Vector3f            color          = math::Vector3f::Ones();
		Real32                    luminousPower  = 1.f;
		Real32                    range          = 1.f;
		Real32                    innerHalfAngle = 22.5f;
		Real32                    outerHalfAngle = 45.f;
		rsc::ELocalLightFlags     flags          = rsc::ELocalLightFlags::Default;
		as::IESProfileAssetHandle iesProfile;
	} def;

	PrefabInstance* owningPrefab = nullptr;

	struct
	{
		rsc::LocalLightHandle light;
	} render;

	LightEntityHandle next;
};


struct WorldLights
{
	WorldLights()
		: entities("World_LightEntitiesPool")
	{
	}

	lib::PagedGenerationalPool<LightEntity> entities;
};

} // spt::gf
