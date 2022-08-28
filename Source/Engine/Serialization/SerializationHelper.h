#pragma

#include "SculptorCoreTypes.h"

#include "YAMLSerializerHelper.h"

#include <fstream>
#include <filesystem>

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
	SPT_PROFILE_FUNCTION();

	YAML::Emitter out;

	out << data;

	std::ofstream stream(filePath, std::ios::trunc);
	stream << out.c_str();

	stream.close();
}

template<typename TStructType>
Bool SerializationHelper::LoadTextStructFromFile(TStructType& data, const lib::String& filePath)
{
	SPT_PROFILE_FUNCTION();

	std::ifstream stream(filePath);
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