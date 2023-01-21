#pragma once

#include "EditorSandboxMacros.h"
#include "SculptorCoreTypes.h"
#include "UITypes.h"
#include "Pipelines/PipelineState.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RenderScene.h"
#include "View/RenderView.h"

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


DS_BEGIN(TestDS, rg::RGDescriptorSetState<TestDS>)
	DS_BINDING(gfx::RWTexture2DBinding<math::Vector4f>, u_texture)
	DS_BINDING(gfx::ConstantBufferBinding<TestViewInfo>, u_viewInfo)
DS_END()


class EDITOR_SANDBOX_API SandboxRenderer
{
public:

	explicit SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow);
	~SandboxRenderer();

	void Tick(Real32 deltaTime);

	lib::SharedPtr<rdr::Semaphore> RenderFrame();

	ui::TextureID GetUITextureID() const;

	const lib::SharedPtr<rdr::Window>& GetWindow() const;

	rsc::RenderView& GetRenderView() const;

	void SetImageSize(const math::Vector2u& imageSize);

	math::Vector2u GetDisplayTextureResolution() const;

	void	SetFov(Real32 fovDegrees);
	Real32	GetFov();

private:

	void InitializeRenderScene();

	void UpdateSceneUITextureForView(const rsc::RenderView& renderView);

	lib::SharedPtr<rdr::Window>			m_window;

	lib::SharedPtr<rdr::Texture>		m_sceneUITexture;
	lib::SharedPtr<rdr::TextureView>	m_sceneUITextureView;

	rsc::RenderScene					m_renderScene;
	lib::UniquePtr<rsc::RenderView>		m_renderView;

	/** Default FOV value used when		scene image have same width as window */
	Real32								m_fovDegrees;
	Real32								m_nearPlane;
};

} // spt::ed