#pragma once

#include "BlackboardMacros.h"
#include "SculptorCoreTypes.h"
#include "Blackboard.h"
#include "Utility/Templates/Callable.h"
#include "SerializationHelper.h"
#include "Serialization.h"


namespace spt::lib
{

struct DataTypeSerializationMetaData
{
	lib::RawCallable<void(Blackboard&)> factory;
	lib::RawCallable<void(srl::Serializer& serializer, lib::Blackboard& blackboard)> serializer;
};


template<typename TDataType>
DataTypeSerializationMetaData CreateDataTypeSerializationMetaData()
{
	const auto factory = [](lib::Blackboard& blackboard)
	{ 
		blackboard.Create<TDataType>();
	};

	const auto serializer = [](srl::Serializer& serializer, lib::Blackboard& blackboard)
	{
		const lib::RuntimeTypeInfo dataType = lib::TypeInfo<TDataType>();

		if (TDataType* dataTypeData = blackboard.Find<TDataType>())
		{
			serializer.Serialize(dataType.name.data(), *dataTypeData);
		}
	};

	const DataTypeSerializationMetaData dataTypeMetaData
	{
		.factory       = factory,
		.serializer    = serializer,
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

	static void SerializeBlackboard(srl::Serializer& serializer, lib::Blackboard& blackboard)
	{
		return SerializeBlackboardImpl(serializer, blackboard);
	}

private:

	static void RegisterDataTypeImpl(const lib::RuntimeTypeInfo& type, const DataTypeSerializationMetaData& metaData);

	static void SerializeBlackboardImpl(srl::Serializer& writer, lib::Blackboard& blackboard);
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


#define SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(TDataType) \
	inline spt::lib::BlackboardTypeSerializationAutoRegistrator<TDataType> SPT_SCOPE_NAME_EVAL(autoRegistrator, __LINE__);
