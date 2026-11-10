#include "LightPrefab.h"
#include "AssetsSystem.h"
#include "World.h"


namespace spt::gf
{

struct CompiledLightEntity
{
	Transform             transform;
	rsc::ELocalLightType  type           = rsc::ELocalLightType::PointLight;
	math::Vector3f        color          = math::Vector3f::Ones();
	Real32                luminousPower  = 1.f;
	Real32                range          = 1.f;
	Real32                innerHalfAngle = 22.5f;
	Real32                outerHalfAngle = 45.f;
	rsc::ELocalLightFlags flags          = rsc::ELocalLightFlags::Default;
	as::ResourcePathID    iesProfile     = as::InvalidResourcePathID;
};

lib::DynamicArray<Byte> PrefabLightEntityDefinition::Compile(as::PrefabCompiler& compiler) const
{
	SPT_CHECK(type < rsc::ELocalLightType::NUM);
	SPT_CHECK(type != rsc::ELocalLightType::SpotLight || innerHalfAngle <= outerHalfAngle);

	rsc::ELocalLightFlags flags = rsc::ELocalLightFlags::None;
	if (contributeToDI)
	{
		lib::AddFlag(flags, rsc::ELocalLightFlags::ContributeToDI);
	}
	if (contributeToScattering)
	{
		lib::AddFlag(flags, rsc::ELocalLightFlags::ContributeToScattering);
	}

	const CompiledLightEntity compiledEntity
	{
		.transform      = Transform{ .location = location, .rotation = rotation, .scale = scale },
		.type           = type,
		.color          = color,
		.luminousPower  = luminousPower,
		.range          = range,
		.innerHalfAngle = innerHalfAngle,
		.outerHalfAngle = outerHalfAngle,
		.flags          = flags,
		.iesProfile     = iesProfile.empty() ? as::InvalidResourcePathID : as::prefab_compiler_api::CreateAssetDependency(compiler, iesProfile)
	};

	lib::DynamicArray<Byte> compiledData(sizeof(CompiledLightEntity));
	std::memcpy(compiledData.data(), &compiledEntity, sizeof(CompiledLightEntity));

	return compiledData;
}

void PrefabLightEntityDefinition::Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData)
{
	SPT_CHECK(compiledData.size() == sizeof(CompiledLightEntity));

	const CompiledLightEntity& compiledLight = *reinterpret_cast<const CompiledLightEntity*>(compiledData.data());

	const LightEntityHandle lightHandle = context.world.lights.entities.Add(LightEntity
		{
			.def =
			{
				.transform      = compiledLight.transform,
				.type           = compiledLight.type,
				.color          = compiledLight.color,
				.luminousPower  = compiledLight.luminousPower,
				.range          = compiledLight.range,
				.innerHalfAngle = compiledLight.innerHalfAngle,
				.outerHalfAngle = compiledLight.outerHalfAngle,
				.flags          = compiledLight.flags,
				.iesProfile     = context.assetsSystem.GetLoadedAsset<as::IESProfileAsset>(compiledLight.iesProfile)
			},
			.owningPrefab = &context.prefabInstance,
			.next         = context.prefabInstance.lights
		});
	context.prefabInstance.lights = lightHandle;

	context.world.entities.SetTypedEntity(context.prefabInstance.entities, entityIdx, lightHandle);
}

} // spt::gf
