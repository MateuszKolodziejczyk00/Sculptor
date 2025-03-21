#pragma once

#include "BlackboardMacros.h"
#include "SculptorCoreTypes.h"
#include "Blackboard.h"
#include "Utility/Templates/Callable.h"
#include "YAMLSerializerHelper.h"
#include "SerializationHelper.h"


namespace spt::lib
{

struct DataTypeSerializationMetaData
{
	lib::RawCallable<void(Blackboard&)> factory;
	lib::RawCallable<void(srl::SerializerWrapper<srl::YAMLEmitter>& emitter, const lib::Blackboard& blackboard)> emitter;
	lib::RawCallable<void(srl::SerializerWrapper<srl::YAMLWriter>& writer, const lib::Blackboard& blackboard)>   serializer;
	lib::RawCallable<void(srl::SerializerWrapper<srl::YAMLLoader>& loader, Blackboard&)>                         deserializer;
};


template<typename TDataType>
DataTypeSerializationMetaData CreateDataTypeSerializationMetaData()
{
	const auto factory = [](lib::Blackboard& blackboard)
	{ 
		blackboard.Create<TDataType>();
	};

	const auto emitter = [](srl::SerializerWrapper<srl::YAMLEmitter>& emitter, const lib::Blackboard& blackboard)
	{
		const lib::RuntimeTypeInfo dataType = lib::TypeInfo<TDataType>();

		if (const TDataType* dataTypeData = blackboard.Find<TDataType>())
		{
			emitter.Serialize(dataType.name.data(), *dataTypeData);
		}
	};

	const auto serializer = [](srl::SerializerWrapper<srl::YAMLWriter>& writer, const lib::Blackboard& blackboard)
	{
		const lib::RuntimeTypeInfo dataType = lib::TypeInfo<TDataType>();

		if (const TDataType* dataTypeData = blackboard.Find<TDataType>())
		{
			writer.Serialize(dataType.name.data(), *dataTypeData);
		}
	};

	const auto deserializer = [](srl::SerializerWrapper<srl::YAMLLoader>& loader, lib::Blackboard& blackboard)
	{
		const lib::RuntimeTypeInfo dataType = lib::TypeInfo<TDataType>();

		TDataType& data = blackboard.GetOrCreate<TDataType>();
		loader.Serialize(dataType.name.data(), data);
	};

	const DataTypeSerializationMetaData dataTypeMetaData
	{
		.factory       = factory,
		.emitter       = emitter,
		.serializer    = serializer,
		.deserializer  = deserializer
	};

	return dataTypeMetaData;
}


class BLACKBOARD_API BlackboardSerializer
{
public:

	template<typename TType>
	static void RegisterDataType()
	{
		const DataTypeSerializationMetaData dataTypeMetaData = CreateDataTypeSerializationMetaData<TType>();
		RegisterDataTypeImpl(lib::TypeInfo<TType>(), dataTypeMetaData);
	}

	static void EmitBlackboard(srl::SerializerWrapper<srl::YAMLEmitter>& emitter, const lib::Blackboard& blackboard)
	{
		return EmitBlackboardImpl(emitter, blackboard);
	}

	static void SerializeBlackboard(srl::SerializerWrapper<srl::YAMLWriter>& writer, const lib::Blackboard& blackboard)
	{
		return SerializeBlackboardImpl(writer, blackboard);
	}

	static void DeserializeBlackboard(srl::SerializerWrapper<srl::YAMLLoader>& loader, lib::Blackboard& blackboard)
	{
		DeserializeBlackboardImpl(loader, blackboard);
	}

private:

	static void RegisterDataTypeImpl(const lib::RuntimeTypeInfo& type, const DataTypeSerializationMetaData& metaData);

	static void EmitBlackboardImpl(srl::SerializerWrapper<srl::YAMLEmitter>& emitter, const lib::Blackboard& blackboard);
	static void SerializeBlackboardImpl(srl::SerializerWrapper<srl::YAMLWriter>& writer, const lib::Blackboard& blackboard);
	static void DeserializeBlackboardImpl(srl::SerializerWrapper<srl::YAMLLoader>& loader, lib::Blackboard& blackboard);
};


template<typename TType>
class BlackboardTypeSerializationAutoRegistrator
{
public:

	BlackboardTypeSerializationAutoRegistrator()
	{
		BlackboardSerializer::RegisterDataType<TType>();
	}
};

} // spt::lib


namespace spt::srl
{

template<>
struct TypeSerializer<lib::Blackboard>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (std::is_same_v<Serializer, srl::YAMLLoader>)
		{
			lib::BlackboardSerializer::DeserializeBlackboard(serializer, data);
		}
		else if constexpr (std::is_same_v<Serializer, srl::YAMLWriter>)
		{
			lib::BlackboardSerializer::SerializeBlackboard(serializer, data);
		}
		else
		{
			lib::BlackboardSerializer::EmitBlackboard(serializer, data);
		}
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::lib::Blackboard)


#define SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(TDataType) \
	inline spt::lib::BlackboardTypeSerializationAutoRegistrator<TDataType> SPT_SCOPE_NAME_EVAL(autoRegistrator, __LINE__);
