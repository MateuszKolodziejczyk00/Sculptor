#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"

#include "Serialization.h"


namespace spt::srl
{

class SerializationHelper
{
public:

	template<typename TStructType>
	static lib::String SerializeStruct(const TStructType& data);

	template<typename TStructType>
	static Bool DeserializeStruct(TStructType& data, const lib::String& serializedData);

	template<typename TStructType>
	static void SaveTextStructToFile(const TStructType& data, const lib::String& filePath);

	static void SaveBinaryToFile(const Byte* data, SizeType dataSize, const lib::String& filePath);

	template<typename TStructType>
	static Bool LoadTextStructFromFile(TStructType& data, const lib::String& filePath);
};

template<typename TStructType>
lib::String SerializationHelper::SerializeStruct(const TStructType& data)
{
	SPT_PROFILER_FUNCTION();

	srl::Serializer serializer = srl::Serializer::CreateWriter();
	const_cast<TStructType&>(data).Serialize(serializer);
	return serializer.ToString();
}

template<typename TStructType>
Bool SerializationHelper::DeserializeStruct(TStructType& data, const lib::String& serializedData)
{
	SPT_PROFILER_FUNCTION();

	srl::Serializer serializer = srl::Serializer::CreateReader(serializedData);
	data.Serialize(serializer);

	return true;
}

template<typename TStructType>
void SerializationHelper::SaveTextStructToFile(const TStructType& data, const lib::String& filePath)
{
	SPT_PROFILER_FUNCTION();

	lib::File::SaveDocument(filePath, SerializeStruct(data));
}

inline void SerializationHelper::SaveBinaryToFile(const Byte* data, SizeType dataSize, const lib::String& filePath)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!data);
	SPT_CHECK(dataSize > 0);

	std::ofstream stream = lib::File::OpenOutputStream(filePath, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::Binary));
	SPT_CHECK(stream.is_open());

	stream.write(reinterpret_cast<const char*>(data), dataSize);

	stream.close();
}

template<typename TStructType>
Bool SerializationHelper::LoadTextStructFromFile(TStructType& data, const lib::String& filePath)
{
	SPT_PROFILER_FUNCTION();

	lib::String serializedData = lib::File::ReadDocument(filePath);

	if (serializedData.empty())
	{
		return false;
	}

	return DeserializeStruct(data, serializedData);
}

} // spt::srl