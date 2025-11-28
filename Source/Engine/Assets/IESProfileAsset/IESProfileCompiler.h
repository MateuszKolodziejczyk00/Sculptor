#pragma once

#include "SculptorCoreTypes.h"
#include "IESProfileTypes.h"


namespace spt::as
{

struct IESProfileCompilationResult
{
	math::Vector2u            resolution;
	lib::DynamicArray<Uint16> textureData;
	// texture data is normalized (0-1) candela values
	// this value represents max candela value in the original IES profile which can be used to recover original values
	Real32                     lightSourceCandela = 1.f;
};


class IESProfileCompiler
{
public:

	IESProfileCompiler() = default;

	IESProfileCompiler& SetResolution(math::Vector2u resolution)
	{
		m_resolution = resolution;
		return *this;
	}

	IESProfileCompilationResult Compile(IESProfileDefinition profileDef) const;

private:

	math::Vector2u m_resolution = math::Vector2u(256, 256);
};

} // spt::as