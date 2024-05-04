#ifndef SHAPES_HLSLI
#define SHAPES_HLSLI

struct Sphere
{
	static Sphere Create(in float3 center, in float radius)
	{
		Sphere s;
		s.center = center;
		s.radius = radius;
		return s;
	}

	float3 center;
	float radius;
};


struct Plane
{
	static Plane Create(in float3 normal, in float3 origin)
	{
		Plane p;
		p.normal = normal;
		p.dist = dot(normal, origin);
		return p;
	}

	float Distance(in float3 location)
	{
		return abs(dot(normal, location) - dist);
	}

	float3 Project(in float3 location)
	{
		return location - normal * Distance(location);
	}

	float3 normal;
	float dist;
};


struct IntersectionResult
{
	bool IsValid()
	{
		return t >= 0.f;
	}

	float GetTime()
	{
		return t;
	}

	operator bool()
	{
		return IsValid();
	}

	bool IsCloserThan(in IntersectionResult other)
	{
		if(other.IsValid())
		{
			return IsValid() ? GetTime() < other.GetTime() : false;
		}

		return IsValid();
	}

	float t;
};


IntersectionResult CreateIntersectionResult(float t)
{
	IntersectionResult res;
	res.t = t;
	return res;
}


IntersectionResult InvalidIntersectionResult()
{
	return CreateIntersectionResult(-1.f);
}


struct Ray
{
	static Ray Create(in float3 origin, in float3 direction)
	{
		Ray r;
		r.origin = origin;
		r.direction = direction;
		return r;
	}

	// Returns the distance along the ray where the intersection occurs.
	// Based on: chapter 22.6.2 of "Real-Time Rendering, 3rd Edition"
	IntersectionResult IntersectSphere(in Sphere sphere)
	{
		const float3 l = sphere.center - origin;
		const float s = dot(l, direction);
		const float l2 = dot(l, l);
		const float r2 = Pow2(sphere.radius);
		if(s < 0.f && l2 > r2)
		{
			return InvalidIntersectionResult();
		}

		const float m2 = l2 - Pow2(s);

		if(m2 > r2)
		{
			return InvalidIntersectionResult();
		}

		const float q = sqrt(r2 - m2);
		const float t = (l2 > r2) ? s - q : s + q;

		return CreateIntersectionResult(t);
	}


	IntersectionResult IntersectPlane(in Plane plane)
	{
		const float denom = dot(plane.normal, direction);
		
		// If the denominator is close to 0, the ray is parallel to the plane
		if(abs(denom) < 0.000001f)
		{
			return InvalidIntersectionResult();
		}
	
		const float t = (plane.dist - dot(plane.normal, origin)) / denom;
	
		// If t is negative, the intersection is behind the ray's origin
		if(t < 0.f)
		{
			return InvalidIntersectionResult();
		}
	
		return CreateIntersectionResult(t);
	}

	float3 GetTimeLocation(float time)
	{
		return origin + direction * time;
	}

	float3 GetIntersectionLocation(in IntersectionResult intersection)
	{
		SPT_CHECK_MSG(intersection.IsValid(), L"Ray::GetIntersectionLocation() Invalid intersection!");
		return GetTimeLocation(intersection.GetTime());
	}

	float3 origin;
	float3 direction;
};

#endif // SHAPES_HLSLI