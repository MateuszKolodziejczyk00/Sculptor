#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "SerializationHelper.h"
#include "Utils/CommandLineArguments.h"


namespace spt::engn
{

struct EngineInitializationParams
{
	EngineInitializationParams()
		: cmdLineArgsNum(0)
		, cmdLineArgs(nullptr)
	{ }

	Uint32		cmdLineArgsNum;
	char**		cmdLineArgs;
};


class ENGINE_CORE_API Engine
{
public:

	static void					Initialize(const EngineInitializationParams& initializationParams);

	static const lib::String&	GetRelPathToEngine();

	// Config helpers ===============================================

	template<typename TDataType>
	static void SaveConfigData(const TDataType& data, const lib::String& configFileName);

	template<typename TDataType>
	static Bool LoadConfigData(TDataType& data, const lib::String& configFileName);

private:

	static Engine& GetInstance();

	Engine() = default;

	EngineInitializationParams	m_initParams;

	CommandLineArguments		m_cmdLineArgs;

	// Cached from cmd line args for faster access
	lib::String					m_relPathToEngineDirectory;
};


template<typename TDataType>
void Engine::SaveConfigData(const TDataType& data, const lib::String& configFileName)
{
	const lib::String finalPath = GetRelPathToEngine() + "Config/" + configFileName;
	srl::SerializationHelper::SaveTextStructToFile(data, finalPath);
}

template<typename TDataType>
Bool Engine::LoadConfigData(TDataType& data, const lib::String& configFileName)
{
	const lib::String finalPath = GetRelPathToEngine() + "Config/" + configFileName;
	return srl::SerializationHelper::LoadTextStructFromFile(data, finalPath);
}

} // spt::engn
