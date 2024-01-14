#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderDebugCommandsExecutor.h"


namespace spt::gfx::dbg
{

namespace shader_command_op
{
static constexpr Uint32 Printf = 1;
} // shader_command_op


class ShaderPrintfExecutor : public ShaderDebugCommandsExecutor
{
public:

	ShaderPrintfExecutor() = default;

	virtual Bool CanExecute(Uint32 opCode) const override;

	virtual void Execute(Uint32 opCode, ShaderDebugCommandsStream& commandsStream) override;

private:

	void ExecutePrintf(ShaderDebugCommandsStream& commandsStream) const;
};

} // spt::gfx::dbg