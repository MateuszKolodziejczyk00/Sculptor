#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Bindless/BindlessTypes.h"
#include "DebugRenderer.h"


namespace spt::gfx::dbg
{

class ShaderDebugCommandsExecutor;


BEGIN_SHADER_STRUCT(ShaderDebugCommandBufferParams)
	SHADER_STRUCT_FIELD(math::Vector2f,                    mouseUV)
	SHADER_STRUCT_FIELD(math::Vector2i,                    mousePositionHalfRes)
	SHADER_STRUCT_FIELD(Uint32,                            bufferSize)
	SHADER_STRUCT_FIELD(GPUDebugRendererData,              dynamicDebugRendererData)
	SHADER_STRUCT_FIELD(GPUDebugRendererData,              persistentDebugRendererData)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,        debugCommandsBuffer)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,        debugCommandsBufferOffset)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, debugOutputTexture) // HDR
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, debugOnScreenOutputTexture) // LDR
END_SHADER_STRUCT();


namespace shader_command_op
{
static constexpr Uint32 None = 1;
} // shader_command_op


struct ShaderDebugParameters
{
	math::Vector2f mouseUV = {};
	math::Vector2u viewportSize  = {};
	Bool           resetPersistentDebugGeometry = false;
};


class GRAPHICS_API ShaderDebugUtils
{
public:
	
	static ShaderDebugUtils& Get();

	void Bind(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters);
	void Unbind(rg::RenderGraphBuilder& graphBuilder);

	void BindCommandsExecutor(lib::SharedPtr<ShaderDebugCommandsExecutor> commandsExecutor);

	const lib::SharedPtr<rdr::TextureView>& GetDebugOutputTexture() const { return m_debugOnScreenOutputTexture; }

	DebugRenderer& GetDynamicDebugRenderer() { return *m_dynamicDebugRenderer; }
	DebugRenderer& GetPersistentDebugRenderer() { return *m_persistentDebugRenderer; }

private:

	ShaderDebugUtils();

	void InitializeDefaultExecutors();
	void CleanupResources();

	void                        PrepareBuffers(rg::RenderGraphBuilder& graphBuilder);
	void                        PrepareDebugOutputTexture(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters);
	lib::SharedRef<rdr::Buffer> ExtractData(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& sourceBuffer);

	void ScheduleCommandsExecution(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& commands, const lib::SharedRef<rdr::Buffer>& size) const;

	lib::SharedPtr<rdr::Buffer> m_debugCommandsBuffer;
	lib::SharedPtr<rdr::Buffer> m_debugCommandsBufferOffset;

	lib::SharedPtr<rdr::TextureView> m_debugOutputTexture;
	lib::SharedPtr<rdr::TextureView> m_debugOnScreenOutputTexture;

	lib::UniquePtr<DebugRenderer> m_dynamicDebugRenderer;
	lib::UniquePtr<DebugRenderer> m_persistentDebugRenderer;

	mutable lib::Lock                                              m_commandsExecutorsLock;
	lib::DynamicArray<lib::SharedPtr<ShaderDebugCommandsExecutor>> m_commandsExecutors;
};


class GRAPHICS_API ShaderDebugScope
{
public:

	ShaderDebugScope(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters);
	~ShaderDebugScope();

	DebugRenderer& GetDynamicDebugRenderer() const { return ShaderDebugUtils::Get().GetDynamicDebugRenderer(); }
	DebugRenderer& GetPersistentDebugRenderer() const { return ShaderDebugUtils::Get().GetPersistentDebugRenderer(); }

	const lib::SharedPtr<rdr::TextureView>& GetDebugOutputTexture() const { return ShaderDebugUtils::Get().GetDebugOutputTexture(); }

private:

	rg::RenderGraphBuilder& m_graphBuilder;
};

} // spt::gfx::dbg
