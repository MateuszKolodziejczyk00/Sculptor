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
#include "RenderGraphResourcesPool.h"

namespace spt::rdr
{
class Texture;
class Window;
class Semaphore;
class TextureView;
} // spt::rdr


namespace spt::engn
{
class FrameContext;
} // spt::engn


namespace spt::rg::capture
{
class RGCaptureSourceContext;
} // spt::rg::capture


namespace spt::ed
{

class EDITOR_SANDBOX_API SandboxRenderer
{
public:

	explicit SandboxRenderer();
	~SandboxRenderer();

	void Tick(Real32 deltaTime);

	void ProcessView(engn::FrameContext& frame, lib::SharedRef<rdr::TextureView> output);

	const lib::SharedPtr<rsc::RenderView>& GetRenderView() const;

	const lib::SharedPtr<rsc::RenderScene>& GetRenderScene();

	rsc::SceneRenderer& GetSceneRenderer();

	void	SetFov(Real32 fovDegrees);
	Real32	GetFov();
	
	void	SetNearPlane(Real32 nearPlane);
	Real32	GetNearPlane();
	
	void	SetFarPlane(Real32 farPlane);
	Real32	GetFarPlane();
	
	void	SetCameraSpeed(Real32 speed);
	Real32	GetCameraSpeed();

	void SetMousePositionOnViewport(const math::Vector2i& mousePosition);
	
	void SetViewportFocused(Bool isFocused);

	void CreateRenderGraphCapture();
	void CreateScreenshot();

	Bool sunMovement = false;
	Real32 sunMovementSpeed = 0.08f;

	Real32 sunAngleYaw = 0.0f;
	Real32 sunAnglePitch = 0.0f;
	Bool sunAngleDirty = false;

private:

	void InitializeRenderScene();

	void PrepareRenderView(math::Vector2u renderingResolution);

	lib::SharedPtr<rsc::RenderScene>	m_renderScene;
	lib::SharedPtr<rsc::RenderView>		m_renderView;

	rsc::SceneRenderer					m_sceneRenderer;

	/** Default FOV value used when		scene image have same width as window */
	Real32								m_fovDegrees;
	Real32								m_nearPlane;
	Real32								m_farPlane;
	
	Real32								m_cameraSpeed;

	math::Vector2i						m_mousePositionOnViewport;

	rg::RenderGraphResourcesPool		m_resourcesPool;

	rsc::RenderSceneEntityHandle m_directionalLightEntity;

	Bool m_wantsCaptureNextFrame;
	Bool m_wantsToCreateScreenshot;

	Bool m_isViewportFocused;

	lib::SharedPtr<rg::capture::RGCaptureSourceContext> m_captureSourceContext;
};

} // spt::ed