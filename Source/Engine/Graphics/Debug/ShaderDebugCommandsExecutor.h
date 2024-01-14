#pragma once

#include "SculptorCoreTypes.h"


namespace spt::gfx::dbg
{

class ShaderDebugCommandsStream;


class ShaderDebugCommandsExecutor
{
public:

	ShaderDebugCommandsExecutor() = default;
	virtual ~ShaderDebugCommandsExecutor() = default;

	virtual Bool CanExecute(Uint32 opCode) const = 0;

	virtual void Execute(Uint32 opCode, ShaderDebugCommandsStream& commandsStream) = 0;
};

} // spt::gfx::dbg
