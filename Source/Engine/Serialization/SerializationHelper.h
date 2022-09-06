#pragma

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"

#include "YAMLSerializerHelper.h"

namespace spt::srl
{

class SerializationHelper
{
public:

	template<typename TStructType>
	static void	SaveTextStructToFile(const TStructType& data, const lib::String& filePath);

	template<typename TStructType>
	static Bool	LoadTextStructFromFile(TStructType& data, const lib::String& filePath);
};

template<typename TStructType>
void SerializationHelper::SaveTextStructToFile(const TStructType& data, const lib::String& filePath)
{
	SPT_PROFILER_FUNCTION();

	YAML::Emitter out;

	out << data;

	std::ofstream stream = lib::File::OpenOutputStream(filePath, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent));
	SPT_CHECK(stream.is_open());
	stream << out.c_str();

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

		const YAML::Node serializedShadersFile = YAML::Load(stringStream.str());

		data = serializedShadersFile.as<TStructType>();

		stream.close();

		return true;
	}

	return false;
}

} // spt::srl