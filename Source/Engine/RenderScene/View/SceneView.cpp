#include "SceneView.h"
#include "MathUtils.h"
#include "Sequences.h"
#include "EngineFrame.h"

namespace spt::rsc
{

namespace jitter
{

static math::Vector2f GetJitter(Uint64 frameIdx, math::Vector2u resolution)
{
	SPT_CHECK(resolution.x() != 0 && resolution.y() != 0);

	const Uint32 sequenceIdx = static_cast<Uint32>(frameIdx & 7);
	const math::Vector2f sequenceVal = math::Vector2f(math::Sequences::Halton<Real32>(sequenceIdx, 2), math::Sequences::Halton<Real32>(sequenceIdx, 3));
	return (sequenceVal * 2.f - math::Vector2f::Constant(1.f)).cwiseProduct(resolution.cast<Real32>().cwiseInverse());
}

} // jitter

SceneView::SceneView()
	: m_projectionMatrix{}
	, m_viewLocation(math::Vector3f::Zero())
	, m_rotation(math::Quaternionf::Identity())
	, m_nearPlane(0.f)
	, m_farPlane(0.f)
	, m_wantsJitter(false)
	, m_jitterIndex(0)
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
	const Real32 b = -2.f / (top - bottom);
	const Real32 c = -1.f / (far - near);

	const Real32 x = -(right + left) / (right - left);
	const Real32 y = -(top + bottom) / (top - bottom);
	const Real32 z = 1.f + near / (far - near);

	m_projectionMatrix = math::Matrix4f{{0.f,	a,		0.f,	x	},
										{0.f,	0.f,	b,		y	},
										{c,		0.f,	0.f,	z	},
										{0.f,	0.f,	0.f,	1.f } };

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

void SceneView::SetRotation(const math::Vector3f& forwardVector)
{
	m_rotation = math::Quaternionf::FromTwoVectors(math::Vector3f(1.f, 0.f, 0.f), forwardVector);
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

Bool SceneView::IsJittering() const
{
	return m_wantsJitter;
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

math::Matrix4f SceneView::GenerateViewProjectionMatrix() const
{
	return m_projectionMatrix * GenerateViewMatrix();
}

math::Matrix4f SceneView::GenerateInverseViewProjectionMatrix() const
{
	return GenerateViewProjectionMatrix().inverse();
}

Real32 SceneView::ComputeProjectedDepth(Real32 linearDepth) const
{
	const math::Vector4f projected = m_projectionMatrix * math::Vector4f(linearDepth, 0.f, 0.f, 1.f);
	return projected.z() / projected.w();
}

void SceneView::SetJittering(Bool enableJittering)
{
	m_wantsJitter = enableJittering;
}

math::Vector2f SceneView::GetCurrentJitter(math::Vector2u resolution) const
{
	return IsJittering() ? jitter::GetJitter(m_jitterIndex, resolution) : math::Vector2f::Zero();
}

Bool SceneView::IsPerspectiveMatrix() const
{
	return math::Utils::IsNearlyZero(m_projectionMatrix.coeff(3, 3));
}

void SceneView::CachePrevFrameRenderingData()
{
	m_prevFrameRenderingData = m_viewRenderingData;
}

void SceneView::UpdateViewRenderingData(math::Vector2u resolution)
{
	const math::Vector2f prevJitter = m_prevFrameRenderingData.jitter;

	++m_jitterIndex;
	const math::Vector2f jitter = GetCurrentJitter(resolution);

	math::Matrix4f projectionMatrixWithJitter = m_projectionMatrix;
	projectionMatrixWithJitter(0, 0) += jitter.x();
	projectionMatrixWithJitter(1, 0) += jitter.y();

	m_viewRenderingData.viewMatrix                    = GenerateViewMatrix();
	m_viewRenderingData.viewProjectionMatrix          = projectionMatrixWithJitter * m_viewRenderingData.viewMatrix;
	m_viewRenderingData.projectionMatrix              = projectionMatrixWithJitter;
	m_viewRenderingData.viewProjectionMatrixNoJitter  = m_projectionMatrix * m_viewRenderingData.viewMatrix;
	m_viewRenderingData.projectionMatrixNoJitter      = m_projectionMatrix;
	m_viewRenderingData.viewLocation                  = m_viewLocation;
	m_viewRenderingData.viewForward                   = GetForwardVector();
	m_viewRenderingData.inverseView                   = m_viewRenderingData.viewMatrix.inverse();
	m_viewRenderingData.inverseProjection             = projectionMatrixWithJitter.inverse();
	m_viewRenderingData.inverseProjectionNoJitter     = m_projectionMatrix.inverse();
	m_viewRenderingData.inverseViewProjection         = m_viewRenderingData.viewProjectionMatrix.inverse();
	m_viewRenderingData.inverseViewProjectionNoJitter = m_viewRenderingData.viewProjectionMatrixNoJitter.inverse();
	m_viewRenderingData.jitter                        = jitter;

	const math::Vector3f upVectorForWSReconstruction = ComputeRayDirection(math::Vector2f(0.f, 1.f), m_viewRenderingData.inverseView) - m_viewRenderingData.viewForward;
	const math::Vector3f rightVectorForWSReconstruction = ComputeRayDirection(math::Vector2f(1.f, 0.f), m_viewRenderingData.inverseView) - m_viewRenderingData.viewForward;

	m_viewRenderingData.upForLinearReconstruction    = upVectorForWSReconstruction;
	m_viewRenderingData.rightForLinearReconstruction = rightVectorForWSReconstruction;
}

void SceneView::UpdateCullingData()
{
	const math::Matrix4f& viewProjection = m_viewRenderingData.viewProjectionMatrix;

	m_viewCullingData.cullingPlanes[0] = viewProjection.row(3) + viewProjection.row(0);	// right plane:		-x < w  ---> w + x > 0
	m_viewCullingData.cullingPlanes[1] = viewProjection.row(3) - viewProjection.row(0);	// left plane:		 x < w  ---> w - x > 0
	m_viewCullingData.cullingPlanes[2] = viewProjection.row(3) + viewProjection.row(1);	// top plane:		-y < w  ---> w + y > 0
	m_viewCullingData.cullingPlanes[3] = viewProjection.row(3) - viewProjection.row(1);	// bottom plane:	 y < w  ---> w - y > 0
	
	if (IsPerspectiveMatrix())
	{
		// Far plane culling. For this one we use cached far plane value because we may not have far plane culling in projection matrix (as we're using infinite far in some cases)
		m_viewCullingData.cullingPlanes[4] = -viewProjection.row(3) + math::RowVector4f(0.f, 0.f, 0.f, m_farPlane);
	}
	else
	{
		m_viewCullingData.cullingPlanes[4] = viewProjection.row(3) - viewProjection.row(2);	// far plane:	z < w  ---> w - z > 0
	}

	// normalize planes (we need normals to compare with bounding spheres radius)
	for (math::Vector4f& cullingPlane : m_viewCullingData.cullingPlanes)
	{
		const Real32 norm = cullingPlane.head<3>().norm();
		cullingPlane /= norm;
	}
}

math::Vector3f SceneView::ComputeRayDirection(math::Vector2f ndc, const math::Matrix4f& inverseView) const
{
	const Real32 x = 1.f;
	const Real32 y = ndc.x() / (m_projectionMatrix(0, 1) - m_projectionMatrix(0, 0));
	const Real32 z = ndc.y() / (m_projectionMatrix(1, 2) - m_projectionMatrix(1, 0));

	const math::Vector3f dir = math::Vector3f(x, y, z);
	return inverseView.topLeftCorner<3, 3>() * dir;
}

} // spt::rsc
