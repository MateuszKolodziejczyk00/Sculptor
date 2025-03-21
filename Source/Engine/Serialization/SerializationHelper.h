#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"

#include "YAMLSerializerHelper.h"

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

	YAML::Emitter out;
	out << data;
	return out.c_str();
}

template<typename TStructType>
Bool SerializationHelper::DeserializeStruct(TStructType& data, const lib::String& serializedData)
{
	SPT_PROFILER_FUNCTION();

	YAML::Node node = YAML::Load(serializedData);

	if (node)
	{
		data = node.as<TStructType>();
		return true;
	}

	return false;
}

template<typename TStructType>
void SerializationHelper::SaveTextStructToFile(const TStructType& data, const lib::String& filePath)
{
	SPT_PROFILER_FUNCTION();

	std::ofstream stream = lib::File::OpenOutputStream(filePath, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent));
	SPT_CHECK(stream.is_open());

	stream << SerializeStruct(data);

	stream.close();
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

	std::ifstream stream = lib::File::OpenInputStream(filePath);
	if (!stream.fail())
	{
		std::stringstream stringStream;
		stringStream << stream.rdbuf();

		DeserializeStruct(data, lib::String(stringStream.str()));

		stream.close();

		return true;
	}

	return false;
}

} // spt::srl