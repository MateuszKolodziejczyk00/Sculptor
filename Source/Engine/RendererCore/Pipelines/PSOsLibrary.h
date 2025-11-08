#pragma once

#include "PSOsLibraryTypes.h"


namespace spt::rdr
{

class RENDERER_CORE_API PSOsLibrary
{
public:

	static PSOsLibrary& GetInstance();

	void RegisterPSO(PSOPrecacheFunction pso);

	void PrecachePSOs(const PSOPrecacheParams& precacheParams);

private:

	PSOsLibrary() = default;

	lib::DynamicArray<PSOPrecacheFunction> m_registeredPSOs;
};

} // spt::rdr