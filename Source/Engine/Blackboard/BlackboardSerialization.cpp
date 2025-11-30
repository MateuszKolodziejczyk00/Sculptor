#include "BlackboardSerialization.h"


namespace spt::lib
{

using BlacboardTypeSerializersRegistry = lib::HashMap<lib::RuntimeTypeInfo, DataTypeSerializationMetaData>;

static BlacboardTypeSerializersRegistry& GetSerializersRegistry()
{
	static BlacboardTypeSerializersRegistry registry;
	return registry;
}


void BlackboardSerializer::RegisterDataTypeImpl(const lib::RuntimeTypeInfo& type, const DataTypeSerializationMetaData& metaData)
{
	GetSerializersRegistry()[type] = metaData;
}

void BlackboardSerializer::SerializeBlackboardImpl(srl::Serializer& serializer, lib::Blackboard& blackboard)
{
	lib::DynamicArray<lib::RuntimeTypeInfo> blackboardContent;

	if (serializer.IsSaving())
	{
		blackboard.ForEachType(
			[&blackboardContent](const lib::RuntimeTypeInfo& type)
			{
				blackboardContent.emplace_back(type);
			});

	}

	serializer.Serialize("BlackboardContent", blackboardContent);

	if (serializer.IsLoading())
	{
		for (const lib::RuntimeTypeInfo& type : blackboardContent)
		{
			const auto& serializers = GetSerializersRegistry();
			const auto foundMetaData = serializers.find(type);
			if (foundMetaData != serializers.cend())
			{
				foundMetaData->second.factory(blackboard);
			}
		}
	}

	blackboard.ForEachType(
		[&serializer, &blackboard](const lib::RuntimeTypeInfo& type)
		{
			const auto& serializers = GetSerializersRegistry();

			const auto foundMetaData = serializers.find(type);
			if (foundMetaData != serializers.cend())
			{
				foundMetaData->second.serializer(serializer, blackboard);
			}
		});
}

} // spt::lib
