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
#include "SceneRenderer/SceneRenderer.h"

namespace spt::rdr
{
class Texture;
class Window;
class Semaphore;
} // spt::rdr

namespace spt::ed
{

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

	rsc::SceneRenderer& GetSceneRenderer();

	void SetImageSize(const math::Vector2u& imageSize);

	math::Vector2u GetDisplayTextureResolution() const;

	void	SetFov(Real32 fovDegrees);
	Real32	GetFov();
	
	void	SetNearPlane(Real32 nearPlane);
	Real32	GetNearPlane();
	
	void	SetFarPlane(Real32 farPlane);
	Real32	GetFarPlane();
	
	void	SetCameraSpeed(Real32 speed);
	Real32	GetCameraSpeed();

private:

	void InitializeRenderScene();

	void UpdateSceneUITextureForView(const rsc::RenderView& renderView);

	lib::SharedPtr<rdr::Window>			m_window;

	lib::SharedPtr<rdr::Texture>		m_sceneUITexture;
	lib::SharedPtr<rdr::TextureView>	m_sceneUITextureView;

	rsc::RenderScene					m_renderScene;
	lib::UniquePtr<rsc::RenderView>		m_renderView;

	rsc::SceneRenderer					m_sceneRenderer;

	/** Default FOV value used when		scene image have same width as window */
	Real32								m_fovDegrees;
	Real32								m_nearPlane;
	Real32								m_farPlane;
	
	Real32								m_cameraSpeed;
};

} // spt::ed