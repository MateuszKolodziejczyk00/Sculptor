#pragma once

#include "SculptorLibMacros.h"
#include "MathCore.h"


namespace spt::lib
{

class SCULPTOR_LIB_API Color
{
public:

	explicit constexpr Color(Real32 inR = 0.f, Real32 inG = 0.f, Real32 inB = 0.f, Real32 inA = 0.f)
		: r(inR)
		, g(inG)
		, b(inB)
		, a(inA)
	{ }

	explicit Color(const math::Vector3f& color, Real32 inA = 0.f)
		: r(color.x())
		, g(color.y())
		, b(color.z())
		, a(inA)
	{ }

	explicit Color(const math::Vector4f& color)
		: r(color.x())
		, g(color.y())
		, b(color.z())
		, a(color.w())
	{ }

	Real32		r;
	Real32		g;
	Real32		b;
	Real32		a;

	static const Color Red;
	static const Color Green;
	static const Color Blue;
};


class CompressedColor
{
public:

	CompressedColor()
		: compressed(0)
	{ }

	explicit CompressedColor(const Color& color)
	{
		Compress(color);
	}

	CompressedColor& operator=(const Color& color)
	{
		Compress(color);
		return *this;
	}

	Color Get() const
	{
		return Decompress();
	}

private:

	void	Compress(const Color& color)
	{
		compressed = 0;
		compressed |= (static_cast<Uint32>(color.r * 255.f) << 0);
		compressed |= (static_cast<Uint32>(color.g * 255.f) << 8);
		compressed |= (static_cast<Uint32>(color.b * 255.f) << 16);
		compressed |= (static_cast<Uint32>(color.a * 255.f) << 24);
	}

	Color	Decompress() const
	{
		Color color;
		color.r = static_cast<Real32>((compressed >> 0) & 255) / 255.f;
		color.g = static_cast<Real32>((compressed >> 8) & 255) / 255.f;
		color.b = static_cast<Real32>((compressed >> 16) & 255) / 255.f;
		color.a = static_cast<Real32>((compressed >> 24) & 255) / 255.f;

		return color;
	}
	
	Uint32		compressed;
};

}
