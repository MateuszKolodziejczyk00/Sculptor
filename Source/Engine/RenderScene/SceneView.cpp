#include "SceneView.h"

namespace spt::rsc
{

SceneView::SceneView()
	: m_projectionMatrix{}
	, m_viewLocation(math::Vector3f::Zero())
	, m_rotation(math::Quaternionf::Identity())
{ }

void SceneView::SetPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near)
{
	const Real32 h = 1.f / std::tan(fovRadians * 0.5f);

	const Real32 a = -h;
	const Real32 b = -h * aspect;

	m_projectionMatrix = math::Matrix4f{{a,		0.f,	0.f,	0.f},
										{0.f,	b,		0.f,	0.f},
										{0.f,	0.f,	0.f,   -1.f},
										{0.f,	0.f,	near,	0.f} };
}

void SceneView::SetOrthographicsProjection(Real32 near, Real32 far, Real32 bottom, Real32 top, Real32 left, Real32 right)
{
	const Real32 a = 2.f / (right - left);
	const Real32 b = 2.f / (top - bottom);
	const Real32 c = 2.f / (far - near);

	const Real32 x = -(right + left) / (right - left);
	const Real32 y = -(top + bottom) / (top - bottom);
	const Real32 z = -(far + near) / (far - near);

	m_projectionMatrix = math::Matrix4f{{a,		0.f,	0.f,	x	},
										{0.f,	b,		0.f,	y	},
										{0.f,	0.f,	c,		z	},
										{0.f,	0.f,	0.f,	1.f	} };
}

const math::Matrix4f& SceneView::GetProjectionMatrix() const
{
	return m_projectionMatrix;
}

void SceneView::Move(const math::Vector3f& delta)
{
	m_viewLocation += delta;
}

void SceneView::SetLocation(const math::Vector3f& newLocation)
{
	m_viewLocation = newLocation;
}

const math::Vector3f& SceneView::GetLocation() const
{
	return m_viewLocation;
}

void SceneView::Rotate(const math::AngleAxisf& delta)
{
	m_rotation = delta * m_rotation;
}

void SceneView::SetRotation(const math::Quaternionf& newRotation)
{
	m_rotation = newRotation;
}

const math::Quaternionf& SceneView::GetRotation() const
{
	return m_rotation;
}

math::Matrix4f SceneView::GenerateViewMatrix() const
{
	const math::Vector3f forward = m_rotation * math::Vector3f::UnitX();
	const math::Vector3f up = m_rotation * math::Vector3f::UnitZ();

	const math::Matrix3f inverseRotation = m_rotation.toRotationMatrix().transpose();
	const math::Vector3f m_invLocation = inverseRotation * -m_viewLocation;

	math::Matrix4f viewMatrix = math::Matrix4f::Zero();
	viewMatrix.topLeftCorner<3, 3>() = inverseRotation;
	viewMatrix.topRightCorner<3, 1>() = m_invLocation;
	viewMatrix(3, 3) = 1.f;

	return viewMatrix;
}

SceneViewData SceneView::GenerateViewData() const
{
	SPT_PROFILER_FUNCTION();

	SceneViewData data;
	data.viewMatrix = GenerateViewMatrix();
	data.viewProjectionMatrix = m_projectionMatrix * data.viewMatrix;
	data.projectionMatrix = m_projectionMatrix;

	return data;
}

} // spt::rsc
