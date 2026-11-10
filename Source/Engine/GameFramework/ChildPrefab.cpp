#include "ChildPrefab.h"
#include "PrefabAsset.h"
#include "AssetsSystem.h"
#include "World.h"


SPT_DEFINE_LOG_CATEGORY(PrefabChildEntity, true);

namespace spt::gf
{

struct CompiledChildEntity
{
	Transform transform;

	as::ResourcePathID prefab = as::InvalidResourcePathID;

	Uint32 overridesNum = 0u;
};


lib::DynamicArray<Byte> PrefabChildEntityDefinition::Compile(as::PrefabCompiler& compiler) const
{
	const Uint32 overridesNum = static_cast<Uint32>(overrides.size());

	CompiledChildEntity compiledEntity;
	compiledEntity.transform.location = location;
	compiledEntity.transform.rotation = rotation;
	compiledEntity.transform.scale    = scale;

	compiledEntity.prefab = as::prefab_compiler_api::CreateAssetDependency(compiler, prefab);

	compiledEntity.overridesNum = overridesNum;

	lib::DynamicArray<Byte> compiledData(sizeof(CompiledChildEntity) + overridesNum * sizeof(as::CompiledPrefabOverride));
	std::memcpy(compiledData.data(), &compiledEntity, sizeof(CompiledChildEntity));

	for (Uint32 overrideIdx = 0u; overrideIdx < overridesNum; ++overrideIdx)
	{
		const as::PrefabParameterOverride& paramOverride = overrides[overrideIdx];

		const lib::DynamicArray<Byte> compiledValue = paramOverride.value->Compile(compiler);

		const as::CompiledPrefabOverride compiledOverride
		{
			.paramKey   = lib::HashedString::HashString(paramOverride.param),
			.valueType  = paramOverride.value->GetType().id,
			.dataOffset = static_cast<Uint32>(compiledData.size()),
			.dataSize   = static_cast<Uint32>(compiledValue.size())
		};

		std::memcpy(compiledData.data() + sizeof(CompiledChildEntity) + overrideIdx * sizeof(as::CompiledPrefabOverride), &compiledOverride, sizeof(as::CompiledPrefabOverride));

		compiledData.insert(compiledData.end(), compiledValue.cbegin(), compiledValue.cend());
	}

	return compiledData;
}

lib::TypeID PrefabChildEntityDefinition::GetParameterType(const as::PrefabParameterResolveContext& context, lib::HashedString::KeyType member, Uint32 elementIdx) const
{
	const as::PrefabAssetHandle childPrefab = context.assetsSystem.GetLoadedAsset<as::PrefabAsset>(context.owningPrefab.ResolveAssetRelativePath(prefab));
	if (!childPrefab.IsValid())
	{
		SPT_LOG_ERROR(PrefabChildEntity, "Failed to get parameter type for PrefabChildEntity - prefab asset ({}) is invalid", prefab.string());
		return lib::TypeID{};
	}

	const lib::TypeID paramType = childPrefab->GetParameterType(member);
	if (paramType == 0u)
	{
		SPT_LOG_ERROR(PrefabChildEntity, "Failed to get parameter type for PrefabChildEntity - prefab asset ({}) has no parameter with name {}", childPrefab->GetName().GetData(), member);
		return lib::TypeID{};
	}

	return paramType;
}

void PrefabChildEntityDefinition::Spawn(SpawnContext& context, Uint32 entityIdx, lib::Span<const Byte> compiledData)
{
	const CompiledChildEntity& compiledChild = *reinterpret_cast<const CompiledChildEntity*>(compiledData.data());

	const as::PrefabAssetHandle prefabAsset = context.assetsSystem.GetLoadedAsset<as::PrefabAsset>(compiledChild.prefab);
	if (!prefabAsset.IsValid())
	{
		SPT_LOG_ERROR(PrefabChildEntity, "Failed to spawn PrefabChildEntity - prefab asset ({}) is invalid", compiledChild.prefab);
		return;
	}

	const Transform& parent = context.transform;
	const Transform& local  = compiledChild.transform;

	const math::Quaternionf parentRotation = math::Utils::EulerToQuaternionDegrees(parent.rotation.x(), parent.rotation.y(), parent.rotation.z());
	const math::Quaternionf localRotation  = math::Utils::EulerToQuaternionDegrees(local.rotation.x(), local.rotation.y(), local.rotation.z());

	const math::Vector3f eulerAngles = (parentRotation * localRotation).toRotationMatrix().eulerAngles(0, 1, 2);

	const PrefabInstanceHandle childHandle = context.world.SpawnPrefab(prefabAsset,
																	   SpawnParams
																	   {
																		   .location = parent.location + parentRotation * parent.scale.cwiseProduct(local.location),
																		   .rotation = math::Vector3f(math::Utils::RadiansToDegrees(eulerAngles.x()), math::Utils::RadiansToDegrees(eulerAngles.y()), math::Utils::RadiansToDegrees(eulerAngles.z())),
																		   .scale    = parent.scale.cwiseProduct(local.scale)
																	   });
	SPT_CHECK(childHandle.IsValid());

	context.world.prefabs.instances.GetRef(childHandle).next = context.prefabInstance.child;
	context.prefabInstance.child = childHandle;

	context.world.entities.SetTypedEntity(context.prefabInstance.entities, entityIdx, childHandle);

	const SetParameterContext childContext
	{
		.world          = context.world,
		.prefabInstance = context.world.prefabs.instances.GetRef(childHandle)
	};

	const lib::Span<const as::CompiledPrefabOverride> overrides(reinterpret_cast<const as::CompiledPrefabOverride*>(compiledData.data() + sizeof(CompiledChildEntity)), compiledChild.overridesNum);
	for (const as::CompiledPrefabOverride& paramOverride : overrides)
	{
		const as::PrefabParameterTypeMetaData* typeMetaData = as::GetPrefabParameterType(paramOverride.valueType);
		SPT_CHECK(!!typeMetaData);

		const lib::Span<const Byte> paramData = compiledData.subspan(paramOverride.dataOffset, paramOverride.dataSize);
		typeMetaData->applier(context.assetsSystem, *prefabAsset, &childContext, paramOverride.paramKey, paramData);
	}
}

void PrefabChildEntityDefinition::ApplyParameter(const SetParameterContext& context, const as::CompiledPrefabBinding& binding, const as::PrefabParameterPayload& payload)
{
	const PrefabInstanceHandle childHandle = context.world.entities.GetTypedEntityHandle<PrefabInstanceHandle>(context.prefabInstance.entities, binding.entityIdx);
	SPT_CHECK(childHandle.IsValid());

	const PrefabInstance& childInstance = context.world.prefabs.instances.GetRef(childHandle);

	const as::PrefabAssetHandle childPrefab = childInstance.prefab;
	SPT_CHECK(childPrefab.IsValid());

	const SetParameterContext childContext
	{
		.world          = context.world,
		.prefabInstance = childInstance
	};

	childPrefab->SetParameter(&childContext, binding.memberKey, payload);
}

} // spt::gf
