#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderDebugCommandsExecutor.h"


namespace spt::gfx::dbg
{

namespace shader_command_op
{
static constexpr Uint32 Printf                 = 1;
static constexpr Uint32 FailedAssertionMessage = 2;
} // shader_command_op


class ShaderMessageCommandsExecutor : public ShaderDebugCommandsExecutor
{
public:

	ShaderMessageCommandsExecutor() = default;

	virtual Bool CanExecute(Uint32 opCode) const override;

	virtual void Execute(Uint32 opCode, ShaderDebugCommandsStream& commandsStream) override;

private:

	void ExecutePrintf(ShaderDebugCommandsStream& commandsStream) const;
	void ExecuteFailedAssertionMessage(ShaderDebugCommandsStream& commandsStream) const;

	lib::String ReadString(ShaderDebugCommandsStream& commandsStream) const;
};

} // spt::gfx::dbg