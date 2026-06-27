#pragma once

#include "GameFrameworkMacros.h"
#include "SculptorCoreTypes.h"
#include "Serialization.h"
#include "MathUtils.h"


namespace spt::gf
{

struct Transform
{
	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();

	math::Affine3f GetAffineTransform() const
	{
		math::Affine3f transform = math::Affine3f::Identity();
		transform.prescale(scale);
		transform.prerotate(math::Utils::EulerToQuaternionDegrees(rotation.x(), rotation.y(), rotation.z()));
		transform.pretranslate(location);
		return transform;
	}

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Location", location);
		serializer.Serialize("Rotation", rotation);
		serializer.Serialize("scale",    scale);
	}
};

} // spt::gf
