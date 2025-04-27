#pragma once

#include "MathCore.h"

#include <optional>


namespace spt::math
{

struct Sphere
{
	Sphere(Vector3f inCenter, Real32 inRadius)
	{
		center = inCenter;
		radius = inRadius;
	}

	Vector3f center;
	Real32   radius;
};


struct Plane
{
	Plane(Vector3f inNormal, Vector3f inOrigin)
	{
		normal = inNormal;
		dist = inOrigin.dot(inNormal);
	}

	Real32 Distance(Vector3f location) const
	{
		return normal.dot(location) - dist;
	}

	Vector3f Project(Vector3f location) const
	{
		return location - normal * Distance(location);
	}

	Vector3f normal;
	Real32   dist;
};


using IntersectionResult = std::optional<Real32>;


struct Ray
{
	Ray(Vector3f inOrigin, Vector3f inDirection)
	{
		origin    = inOrigin;
		direction = inDirection;
	}

	// Returns the distance along the ray where the intersection occurs.
	// Based on: chapter 22.6.2 of "Real-Time Rendering, 3rd Edition"
	IntersectionResult IntersectSphere(Sphere sphere) const
	{
		const Vector3f l = sphere.center - origin;
		const Real32 s = l.dot(direction);
		const Real32 l2 = l.dot(l);
		const Real32 r2 = sphere.radius * sphere.radius;
		if(s < 0.f && l2 > r2)
		{
			return std::nullopt;
		}

		const Real32 m2 = l2 - s * s;

		if(m2 > r2)
		{
			return std::nullopt;
		}

		const Real32 q = sqrt(r2 - m2);
		const Real32 t = (l2 > r2) ? s - q : s + q;

		return t;
	}


	//IntersectionResult IntersectPlane(Plane plane)
	//{
	//	const Real32 denom = dot(plane.normal, direction);
	//	
	//	// If the denominator is close to 0, the ray is parallel to the plane
	//	if(abs(denom) < 0.000001f)
	//	{
	//		return InvalidIntersectionResult();
	//	}
	//
	//	const Real32 t = (plane.dist - dot(plane.normal, origin)) / denom;
	//
	//	// If t is negative, the intersection is behind the ray's origin
	//	if(t < 0.f)
	//	{
	//		return InvalidIntersectionResult();
	//	}
	//
	//	return CreateIntersectionResult(t);
	//}

	Vector3f GetTimeLocation(Real32 time) const
	{
		return origin + direction * time;
	}

	Vector3f origin;
	Vector3f direction;
};



} // spt::math