#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Paths.h"
#include "SerializationHelper.h"


namespace spt::engn
{

class ENGINE_CORE_API ConfigUtils
{
public:

	template<typename TDataType>
	static void SaveConfigData(const TDataType& data, const lib::String& configFileName);

	template<typename TDataType>
	static Bool LoadConfigData(TDataType& data, const lib::String& configFileName);
};


template<typename TDataType>
void ConfigUtils::SaveConfigData(const TDataType& data, const lib::String& configFileName)
{
	const lib::String finalPath = Paths::Combine(Paths::GetConfigsPath(), configFileName);
	srl::SerializationHelper::SaveTextStructToFile(data, finalPath);
}

template<typename TDataType>
Bool ConfigUtils::LoadConfigData(TDataType& data, const lib::String& configFileName)
{
	const lib::String finalPath = Paths::Combine(Paths::GetConfigsPath(), configFileName);
	const Bool success = srl::SerializationHelper::LoadTextStructFromFile(data, finalPath);

	// Save config in case of change of the struct
	SaveConfigData(data, configFileName);

	return success;
}

} // spt::engn
