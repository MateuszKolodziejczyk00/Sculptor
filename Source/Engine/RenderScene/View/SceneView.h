#pragma once

#include "RenderSceneMacros.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SceneViewData)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, projectionMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrixNoJitter)
	SHADER_STRUCT_FIELD(math::Matrix4f, projectionMatrixNoJitter)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, inverseView)
	SHADER_STRUCT_FIELD(math::Matrix4f, inverseProjection)
	SHADER_STRUCT_FIELD(math::Matrix4f, inverseProjectionNoJitter)
	SHADER_STRUCT_FIELD(math::Matrix4f, inverseViewProjection)
	SHADER_STRUCT_FIELD(math::Matrix4f, inverseViewProjectionNoJitter)
	SHADER_STRUCT_FIELD(math::Vector3f, viewLocation)
	SHADER_STRUCT_FIELD(math::Vector3f, viewForward)
	SHADER_STRUCT_FIELD(math::Vector2f, jitter)
	SHADER_STRUCT_FIELD(math::Vector3f, upForLinearReconstruction)
	SHADER_STRUCT_FIELD(math::Vector3f, rightForLinearReconstruction)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SceneViewCullingData)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<math::Vector4f, 5>), cullingPlanes)
END_SHADER_STRUCT();


namespace projection
{

math::Matrix4f CreatePerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near, Real32 far);
math::Matrix4f CreateOrthographicsProjection(Real32 near, Real32 far, Real32 bottom, Real32 top, Real32 left, Real32 right);

} // projection


class RENDER_SCENE_API SceneView
{
public:

	SceneView();

	void					SetPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near, Real32 far);
	void					SetShadowPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near, Real32 far);
	void					SetOrthographicsProjection(Real32 near, Real32 far, Real32 bottom, Real32 top, Real32 left, Real32 right);
	const math::Matrix4f&	GetProjectionMatrix() const;

	void					Move(const math::Vector3f& delta);
	void					SetLocation(const math::Vector3f& newLocation);
	const math::Vector3f&	GetLocation() const;

	void						Rotate(const math::AngleAxisf& delta);
	void						SetRotation(const math::Quaternionf& newRotation);
	void						SetRotation(const math::Vector3f& forwardVector);
	const math::Quaternionf&	GetRotation() const;

	const math::Vector3f		GetForwardVector() const;

	Real32					GetNearPlane() const;
	std::optional<Real32>	GetFarPlane() const;

	void           SetJitter(math::Vector2f jitter);
	void           ResetJitter();
	math::Vector2f GetJitter() const;

	const SceneViewData&		GetViewRenderingData() const;
	const SceneViewCullingData&	GetCullingData() const;

	const SceneViewData&		GetPrevFrameRenderingData() const;

	Bool IsSphereOverlappingFrustum(const math::Vector3f& center, Real32 radius) const;

	math::Matrix4f GenerateViewMatrix() const;

	math::Matrix4f GenerateViewProjectionMatrix() const;
	math::Matrix4f GenerateInverseViewProjectionMatrix() const;

	Real32 ComputeProjectedDepth(Real32 linearDepth) const;

protected:

	Bool IsPerspectiveMatrix() const;

	void CachePrevFrameRenderingData();
	void UpdateViewRenderingData(math::Vector2u resolution);
	void UpdateCullingData();

	math::Vector3f ComputeRayDirection(math::Vector2f ndc, const math::Matrix4f& inverseView) const;

private:

	math::Matrix4f		m_projectionMatrix = {};
	math::Vector3f		m_viewLocation     = math::Vector3f::Zero();
	math::Quaternionf	m_rotation         = math::Quaternionf::Identity();

	Real32					m_nearPlane = 0.f;
	Real32					m_farPlane  = 0.f;

	SceneViewData			m_viewRenderingData = {};
	SceneViewCullingData	m_viewCullingData   = {};

	SceneViewData			m_prevFrameRenderingData = {};
};

} // spt::rsc