#pragma once

#include "EditorSandboxMacros.h"
#include "SculptorCoreTypes.h"
#include "UITypes.h"
#include "Pipelines/PipelineState.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"

#include "SculptorECS.h"
#include "RenderScene.h"

namespace spt::rdr
{
class Texture;
class Window;
class Semaphore;
} // spt::rdr

namespace spt::ed
{

BEGIN_SHADER_STRUCT(, TestViewInfo)
	SHADER_STRUCT_FIELD(math::Vector4f, test1)
	SHADER_STRUCT_FIELD(math::Vector3f, test2)
	SHADER_STRUCT_FIELD(math::Vector4f, test3)
	SHADER_STRUCT_FIELD(Real32, test4)
	SHADER_STRUCT_FIELD(math::Vector2f, test5)
	SHADER_STRUCT_FIELD(math::Vector4f, color)
END_SHADER_STRUCT();


DS_BEGIN(, TestDS, rg::RGDescriptorSetState<TestDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(gfx::RWTexture2DBinding<math::Vector4f>, u_texture)
	DS_BINDING(gfx::ConstantBufferBinding<TestViewInfo>, u_viewInfo)
DS_END()


class EDITOR_SANDBOX_API SandboxRenderer
{
public:

	explicit SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow);

	void Tick(Real32 deltaTime);

	lib::SharedPtr<rdr::Semaphore> RenderFrame();

	ui::TextureID GetUITextureID() const;

	TestDS& GetDescriptorSet();

	const lib::SharedPtr<rdr::Window>& GetWindow() const;

private:

	lib::SharedPtr<rdr::Window>		m_window;

	lib::SharedPtr<TestDS>			m_descriptorSet;
	lib::SharedPtr<rdr::Texture>	m_texture;
	ui::TextureID					m_uiTextureID;
	rdr::PipelineStateID			m_computePipelineID;

	entt::registry					m_registry;
	rsc::RenderScene				m_renderScene;
};

} // spt::ed