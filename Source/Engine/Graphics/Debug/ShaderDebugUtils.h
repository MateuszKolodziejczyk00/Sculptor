#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::gfx::dbg
{

class ShaderDebugCommandsExecutor;


BEGIN_SHADER_STRUCT(ShaderDebugCommandBufferParams)
	SHADER_STRUCT_FIELD(math::Vector2i, mousePosition)
	SHADER_STRUCT_FIELD(math::Vector2i, mousePositionHalfRes)
	SHADER_STRUCT_FIELD(Uint32, bufferSize)
END_SHADER_STRUCT();


DS_BEGIN(ShaderDebugCommandBufferDS, rg::RGDescriptorSetState<ShaderDebugCommandBufferDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                     u_debugCommandsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                     u_debugCommandsBufferOffset)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ShaderDebugCommandBufferParams>), u_debugCommandsBufferParams)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),             u_pageFaultTriggerBuffer)
DS_END();



namespace shader_command_op
{
static constexpr Uint32 None = 1;
} // shader_command_op


struct ShaderDebugParameters
{
	math::Vector2i mousePosition;
};


class ShaderDebugUtils
{
public:
	
	static ShaderDebugUtils& Get();

	void Bind(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters);
	void Unbind(rg::RenderGraphBuilder& graphBuilder);

	void BindCommandsExecutor(lib::SharedPtr<ShaderDebugCommandsExecutor> commandsExecutor);

private:

	ShaderDebugUtils();

	void InitializeDefaultExecutors();
	void CleanupResources();

	void SetDebugParameters(const ShaderDebugParameters& debugParameters);

	void                        PrepareBuffers(rg::RenderGraphBuilder& graphBuilder);
	lib::SharedRef<rdr::Buffer> ExtractData(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& sourceBuffer);

	void ScheduleCommandsExecution(const lib::SharedRef<rdr::Buffer>& commands, const lib::SharedRef<rdr::Buffer>& size) const;

	lib::MTHandle<ShaderDebugCommandBufferDS> m_ds;

	lib::SharedPtr<rdr::Buffer> m_debugCommandsBuffer;
	lib::SharedPtr<rdr::Buffer> m_debugCommandsBufferOffset;

	mutable lib::Lock                                              m_commandsExecutorsLock;
	lib::DynamicArray<lib::SharedPtr<ShaderDebugCommandsExecutor>> m_commandsExecutors;
};


class GRAPHICS_API ShaderDebugScope
{
public:

	ShaderDebugScope(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters);

	~ShaderDebugScope();

private:

	rg::RenderGraphBuilder& m_graphBuilder;
};

} // spt::gfx::dbg