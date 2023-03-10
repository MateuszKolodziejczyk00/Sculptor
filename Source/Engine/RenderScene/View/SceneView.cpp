#include "SceneView.h"

namespace spt::rsc
{

SceneView::SceneView()
	: m_projectionMatrix{}
	, m_viewLocation(math::Vector3f::Zero())
	, m_rotation(math::Quaternionf::Identity())
	, m_nearPlane(0.f)
	, m_farPlane(0.f)
{ }

void SceneView::SetPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near, Real32 far)
{
	const Real32 h = 1.f / std::tan(fovRadians * 0.5f);

	const Real32 a = h;
	const Real32 b = -h * aspect;

	m_projectionMatrix = math::Matrix4f{{0.f,	a,		0.f,	0.f},
										{0.f,	0.f,	b,		0.f},
										{0.f,	0.f,	0.f,    near},
										{1.f,	0.f,	0.f,	0.f} };

	m_nearPlane	= near;
	m_farPlane	= far;
}

void SceneView::SetShadowPerspectiveProjection(Real32 fovRadians, Real32 aspect, Real32 near, Real32 far)
{
	const Real32 h = 1.f / std::tan(fovRadians * 0.5f);

	const Real32 a = h;
	const Real32 b = -h * aspect;

	const Real32 c = -near / (far - near);
	const Real32 d = -far * c;

	m_projectionMatrix = math::Matrix4f{{0.f,	a,		0.f,	0.f},
										{0.f,	0.f,	b,		0.f},
										{c,		0.f,	0.f,    d  },
										{1.f,	0.f,	0.f,	0.f} };

	m_nearPlane	= near;
	m_farPlane	= far;
}

void SceneView::SetOrthographicsProjection(Real32 near, Real32 far, Real32 bottom, Real32 top, Real32 left, Real32 right)
{
	const Real32 a = 2.f / (right - left);
	const Real32 b = 2.f / (top - bottom);
	const Real32 c = 2.f / (far - near);

	const Real32 x = -(right + left) / (right - left);
	const Real32 y = -(top + bottom) / (top - bottom);
	const Real32 z = -(far + near) / (far - near);

	m_projectionMatrix = math::Matrix4f{{0.f,	a,		0.f,	x	},
										{0.f,	0.f,	b,		y	},
										{c,		0.f,	0.f,	z	},
										{0.f,	0.f,	0.f,	1.f	} };

	m_nearPlane	= near;
	m_farPlane	= far;
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

const spt::math::Vector3f SceneView::GetForwardVector() const
{
	return m_rotation * math::Vector3f::UnitX();
}

Real32 SceneView::GetNearPlane() const
{
	return m_nearPlane;
}

std::optional<Real32> SceneView::GetFarPlane() const
{
	return m_farPlane;
}

const SceneViewData& SceneView::GetViewRenderingData() const
{
	return m_viewRenderingData;
}

const SceneViewCullingData& SceneView::GetCullingData() const
{
	return m_viewCullingData;
}

const SceneViewData& SceneView::GetPrevFrameRenderingData() const
{
	return m_prevFrameRenderingData;
}

void SceneView::OnBeginRendering()
{
	SPT_PROFILER_FUNCTION();

	m_prevFrameRenderingData = m_viewRenderingData;

	UpdateViewRenderingData();
	UpdateCullingData();
}

math::Matrix4f SceneView::GenerateViewProjectionMatrix() const
{
	return m_projectionMatrix * GenerateViewMatrix();
}

Bool SceneView::IsPerspectiveMatrix() const
{
	return math::Utils::IsNearlyZero(m_projectionMatrix.coeff(3, 3));
}

void SceneView::UpdateViewRenderingData()
{
	m_viewRenderingData.viewMatrix = GenerateViewMatrix();
	m_viewRenderingData.viewProjectionMatrix = m_projectionMatrix * m_viewRenderingData.viewMatrix;
	m_viewRenderingData.projectionMatrix = m_projectionMatrix;
	m_viewRenderingData.viewLocation = m_viewLocation;
}

void SceneView::UpdateCullingData()
{
	const math::Matrix4f& viewProjection = m_viewRenderingData.viewProjectionMatrix;

	m_viewCullingData.cullingPlanes[0] = viewProjection.row(3) + viewProjection.row(0);	// right plane:		 x < w  ---> w + x > 0
	m_viewCullingData.cullingPlanes[1] = viewProjection.row(3) - viewProjection.row(0);	// left plane:		-x < w  ---> w - x > 0
	m_viewCullingData.cullingPlanes[2] = viewProjection.row(3) + viewProjection.row(1);	// top plane:		 y < w  ---> w + y > 0
	m_viewCullingData.cullingPlanes[3] = viewProjection.row(3) - viewProjection.row(1);	// bottom plane:	-y < w  ---> w - y > 0
	
	if (IsPerspectiveMatrix())
	{
		// Far plane culling. For this one we use cached far plane value because we may not have far plane culling in projection matrix (as we're using infinite far in some cases)
		m_viewCullingData.cullingPlanes[4] = -viewProjection.row(3) + math::RowVector4f(0.f, 0.f, 0.f, m_farPlane);
	}
	else
	{
		m_viewCullingData.cullingPlanes[4] = viewProjection.row(3) + viewProjection.row(2);	// far plane:	z < w  ---> w + z > 0
	}

	// normalize planes (we need normals to compare with bounding spheres radius)
	for (math::Vector4f& cullingPlane : m_viewCullingData.cullingPlanes)
	{
		const Real32 norm = cullingPlane.head<3>().norm();
		cullingPlane /= norm;
	}
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

Bool SceneView::IsSphereOverlappingFrustum(const math::Vector3f& center, Real32 radius) const
{
	math::Vector4f center4 = math::Vector4f::Ones();
	center4.head<3>() = center;

    for(Uint32 planeIdx = 0; planeIdx < 4; ++planeIdx)
    {
		if (m_viewCullingData.cullingPlanes[planeIdx].dot(center4) < -radius)
		{
			return false;
		}
    }

	return true;
}

} // spt::rsc
