#pragma once

#include "RenderSceneMacros.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SceneViewData)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, projectionMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewMatrix)
	SHADER_STRUCT_FIELD(math::Vector3f, viewLocation)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SceneViewCullingData)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<math::Vector4f, 4>), cullingPlanes)
END_SHADER_STRUCT();


class RENDER_SCENE_API SceneView
{
public:

	SceneView();

	void					SetPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near);
	void					SetOrthographicsProjection(Real32 near, Real32 far, Real32 bottom, Real32 top, Real32 left, Real32 right);
	const math::Matrix4f&	GetProjectionMatrix() const;

	void					Move(const math::Vector3f& delta);
	void					SetLocation(const math::Vector3f& newLocation);
	const math::Vector3f&	GetLocation() const;

	void						Rotate(const math::AngleAxisf& delta);
	void						SetRotation(const math::Quaternionf& newRotation);
	const math::Quaternionf&	GetRotation() const;

	math::Matrix4f GenerateViewMatrix() const;

	SceneViewData GenerateViewData() const;

	SceneViewCullingData GenerateCullingData(const SceneViewData& viewData) const;

private:

	math::Matrix4f		m_projectionMatrix;
	math::Vector3f		m_viewLocation;
	math::Quaternionf	m_rotation;
};

} // spt::rsc