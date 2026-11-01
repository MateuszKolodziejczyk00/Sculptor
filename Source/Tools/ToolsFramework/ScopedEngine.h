#pragma once

#include "SculptorCoreTypes.h"


namespace spt::tools
{

struct ScopedEngineParams
{
	Bool initializeAssetsSystem = false;
	Bool initializeModules      = false;

	lib::DynamicArray<lib::String> additionalArgs;
};


class ScopedEngine
{
public:

	explicit ScopedEngine(const ScopedEngineParams& params = ScopedEngineParams{});
	~ScopedEngine();

	ScopedEngine(const ScopedEngine&) = delete;
	ScopedEngine& operator=(const ScopedEngine&) = delete;

private:

	ScopedEngineParams m_params;
};

} // spt::tools
