#pragma once

#include "PrefabAsset.h"
#include "AssetsSystem.h"


namespace spt::as
{

template<typename TParameterType>
struct PrefabParameterTraits
{
	using Authored = TParameterType;
	using Compiled = TParameterType;
	using Runtime  = TParameterType;

	static Compiled Compile(PrefabCompiler& compiler, const Authored& value)  { return value; }
	static Runtime  Decode(AssetsSystem& assetsSystem, const Compiled& value) { return value; }
};


template<typename TAssetType> requires std::derived_from<TAssetType, AssetInstance>
struct PrefabParameterTraits<TAssetType>
{
	using Authored = lib::Path;
	using Compiled = ResourcePathID;
	using Runtime  = TypedAssetHandle<TAssetType>;

	static Compiled Compile(PrefabCompiler& compiler, const Authored& path)
	{
		return prefab_compiler_api::CreateAssetDependency(compiler, path);
	}

	static Runtime Decode(AssetsSystem& assetsSystem, const Compiled& assetID)
	{
		Runtime asset = assetsSystem.GetLoadedAsset<TAssetType>(assetID);
		SPT_CHECK(asset.IsValid());
		return asset;
	}
};


struct PrefabParameterValue
{
	virtual ~PrefabParameterValue() = default;

	virtual lib::RuntimeTypeInfo    GetType() const = 0;
	virtual lib::DynamicArray<Byte> Compile(PrefabCompiler& compiler) const = 0;
	virtual void                    Serialize(srl::Serializer& serializer) = 0;
};


template<typename TParameterType>
struct TypedPrefabParameterValue final : public PrefabParameterValue
{
	using Traits = PrefabParameterTraits<TParameterType>;

	typename Traits::Authored value;

	virtual lib::RuntimeTypeInfo GetType() const override { return lib::TypeInfo<TParameterType>(); }

	virtual lib::DynamicArray<Byte> Compile(PrefabCompiler& compiler) const override
	{
		const typename Traits::Compiled compiledValue = Traits::Compile(compiler, value);

		lib::DynamicArray<Byte> compiledData(sizeof(compiledValue));
		std::memcpy(compiledData.data(), &compiledValue, sizeof(compiledValue));

		return compiledData;
	}

	virtual void Serialize(srl::Serializer& serializer) override
	{
		serializer.Serialize("Value", value);
	}
};


using PrefabParameterValueFactory = lib::RawCallable<lib::UniquePtr<PrefabParameterValue>()>;

using PrefabOverrideApplier = lib::RawCallable<void(AssetsSystem& /* assetsSystem */, const PrefabAsset& /* prefab */, const void* /* context */, lib::HashedString::KeyType /* paramKey */, lib::Span<const Byte> /* compiledValue */)>;


struct PrefabParameterTypeMetaData
{
	PrefabParameterValueFactory factory;
	PrefabOverrideApplier       applier;
};


void                               PREFAB_ASSET_API RegisterPrefabParameterType(lib::TypeID type, const PrefabParameterTypeMetaData& metaData);
const PrefabParameterTypeMetaData* PREFAB_ASSET_API GetPrefabParameterType(lib::TypeID type);


template<typename TParameterType>
struct PrefabParameterTypeRegistrator
{
	PrefabParameterTypeRegistrator()
	{
		using Traits = PrefabParameterTraits<TParameterType>;

		SPT_STATIC_CHECK(std::is_trivially_copyable_v<typename Traits::Compiled>);

		RegisterPrefabParameterType(lib::TypeInfo<TParameterType>::id, PrefabParameterTypeMetaData
			{
				.factory = []{ return lib::UniquePtr<PrefabParameterValue>{ new TypedPrefabParameterValue<TParameterType>() }; },
				.applier = [](AssetsSystem& assetsSystem, const PrefabAsset& prefab, const void* context, lib::HashedString::KeyType paramKey, lib::Span<const Byte> compiledValue)
				{
					SPT_CHECK(compiledValue.size() == sizeof(typename Traits::Compiled));

					typename Traits::Compiled compiled;
					std::memcpy(&compiled, compiledValue.data(), sizeof(compiled));

					const typename Traits::Runtime value = Traits::Decode(assetsSystem, compiled);
					prefab.SetParameter(context, paramKey, PrefabParameterPayload{ .type = lib::TypeInfo<typename Traits::Runtime>::id, .data = &value });
				}
			});
	}
};
#define SPT_REGISTER_PREFAB_PARAMETER_TYPE(type) \
static spt::as::PrefabParameterTypeRegistrator<type> SPT_IDENTIFIER_NAME(parameterTypeRegistrator);


struct PrefabParameterOverride
{
	lib::String param;

	lib::UniquePtr<PrefabParameterValue> value;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Param", param);

		if (serializer.IsSaving())
		{
			lib::RuntimeTypeInfo type = value->GetType();
			serializer.Serialize("Type", type);
		}
		else
		{
			lib::RuntimeTypeInfo type;
			serializer.Serialize("Type", type);

			const PrefabParameterTypeMetaData* typeMetaData = GetPrefabParameterType(type.id);
			SPT_CHECK_MSG(!!typeMetaData, "Prefab parameter type {} is not registered!", type.name.data());

			value = typeMetaData->factory();
		}

		value->Serialize(serializer);
	}
};

} // spt::as
