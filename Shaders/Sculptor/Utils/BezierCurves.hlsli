#ifndef BEZIER_CURVES_HLSLI
#define BEZIER_CURVES_HLSLI

namespace Bezier
{

namespace Cubic
{

template<typename T>
struct Curve
{
	T p0;
	T p1;
	T p2;
	T p3;

	T Evaluate(in float t)
	{
		const float oneMinusT = 1.f - t;
		return oneMinusT * oneMinusT * oneMinusT * p0 +
			   3.f * oneMinusT * oneMinusT * t * p1 +
			   3.f * oneMinusT * t * t * p2 +
			   t * t * t * p3;
	}

	T EvaluateDerivative(in float t)
	{
		const float oneMinusT = 1.f - t;
		return 3.f * oneMinusT * oneMinusT * (p1 - p0) +
			   6.f * oneMinusT * t * (p2 - p1) +
			   3.f * t * t * (p3 - p2);
	}
};

} // namespace Cubic

} // namespace Bezier

#endif // BEZIER_CURVES_HLSLI
