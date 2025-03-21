#include "BlackboardSerialization.h"


namespace spt::lib
{

lib::HashMap<lib::RuntimeTypeInfo, DataTypeSerializationMetaData> typeToSerializationMetaData;


void BlackboardSerializer::RegisterDataTypeImpl(const lib::RuntimeTypeInfo& type, const DataTypeSerializationMetaData& metaData)
{
	typeToSerializationMetaData[type] = metaData;
}

void BlackboardSerializer::EmitBlackboardImpl(srl::SerializerWrapper<srl::YAMLEmitter>& emitter, const lib::Blackboard& blackboard)
{
	blackboard.ForEachType(
		[&emitter, &blackboard](const lib::RuntimeTypeInfo& type)
		{
			const auto foundMetaData = typeToSerializationMetaData.find(type);
			if (foundMetaData != typeToSerializationMetaData.cend())
			{
				foundMetaData->second.emitter(emitter, blackboard);
			}
		});
}

void BlackboardSerializer::SerializeBlackboardImpl(srl::SerializerWrapper<srl::YAMLWriter>& writer, const lib::Blackboard& blackboard)
{
	blackboard.ForEachType(
		[&writer, &blackboard](const lib::RuntimeTypeInfo& type)
		{
			const auto foundMetaData = typeToSerializationMetaData.find(type);
			if (foundMetaData != typeToSerializationMetaData.cend())
			{
				foundMetaData->second.serializer(writer, blackboard);
			}
		});
}

void BlackboardSerializer::DeserializeBlackboardImpl(srl::SerializerWrapper<srl::YAMLLoader>& loader, lib::Blackboard& blackboard)
{
	const YAML::Node& node = loader.Get().GetNode();

	SPT_CHECK(node.IsMap());

	for(YAML::const_iterator it=node.begin();it != node.end();++it)
	{
		const lib::String key = it->first.as<lib::String>();
		const lib::RuntimeTypeInfo type = lib::RuntimeTypeInfo::CreateFromName(key);

		const auto foundMetaData = typeToSerializationMetaData.find(type);
		SPT_CHECK(foundMetaData != typeToSerializationMetaData.cend());

		foundMetaData->second.deserializer(loader, blackboard);
	}
}

} // spt::lib
