#pragma once

#include "EditorSandboxMacros.h"
#include "SculptorCoreTypes.h"
#include "UITypes.h"
#include "Pipelines/PipelineState.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Bindings/StorageTextureBinding.h"
#include "Bindings/ConstantBufferBinding.h"

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


DS_BEGIN(, TestDS, rdr::DescriptorSetState, rhi::EShaderStageFlags::Compute)
	DS_BINDING(rdr::StorageTexture2DBinding<math::Vector4f>, u_texture)
	DS_BINDING(rdr::ConstantBufferBinding<TestViewInfo>, u_viewInfo)
DS_END()


class EDITOR_SANDBOX_API SandboxRenderer
{
public:

	explicit SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow);

	lib::SharedPtr<rdr::Semaphore> RenderFrame();

	ui::TextureID GetUITextureID() const;

	TestDS& GetDescriptorSet();

private:

	lib::SharedPtr<rdr::Window>		m_window;

	lib::SharedPtr<TestDS>			m_descriptorSet;
	lib::SharedPtr<rdr::Texture>	m_texture;
	ui::TextureID					m_uiTextureID;
	rdr::PipelineStateID			m_computePipelineID;
};

} // spt::ed