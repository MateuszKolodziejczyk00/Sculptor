#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::gfx::dbg
{

class ShaderDebugCommandsExecutor;


class ShaderDebugCommandsProcessor
{
public:

	ShaderDebugCommandsProcessor(lib::SharedRef<rdr::Buffer> commands, lib::SharedRef<rdr::Buffer> size);

	void BindExecutors(lib::DynamicArray<lib::SharedPtr<ShaderDebugCommandsExecutor>> executors);

	void ProcessCommands();

private:

	lib::SharedRef<rdr::Buffer> m_commands;
	lib::SharedRef<rdr::Buffer> m_size;

	lib::DynamicArray<lib::SharedPtr<ShaderDebugCommandsExecutor>> m_executors;
};

} // spt::gfx::dbg