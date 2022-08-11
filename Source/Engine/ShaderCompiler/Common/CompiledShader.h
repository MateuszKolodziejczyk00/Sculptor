#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

struct CompiledShader
{
public:

	using Binary = lib::DynamicArray<Uint32>;

	CompiledShader();

	Bool			IsValid() const;

	void			SetBinary(Binary binary);
	const Binary&	GetBinary() const;

private:

	Binary			m_binary;
};

}
