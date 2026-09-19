#ifndef BEZIER_CURVES_HLSLI
#define BEZIER_CURVES_HLSLI

namespace Bezier
{

namespace Cubic
{

struct Curve<T : IFloat>
{
	T p0;
	T p1;
	T p2;
	T p3;

	
T Evaluate(in float t)
{
    const T tt = T(t);
    const T oneMinusT = T(1.0f) - tt;
    const T three = T(3.0f);

    return oneMinusT * oneMinusT * oneMinusT * p0 +
           three * oneMinusT * oneMinusT * tt * p1 +
           three * oneMinusT * tt * tt * p2 +
           tt * tt * tt * p3;
}

T EvaluateDerivative(in float t)
{
    const T tt = T(t);
    const T oneMinusT = T(1.0f) - tt;
    const T three = T(3.0f);
    const T six = T(6.0f);

    return three * oneMinusT * oneMinusT * (p1 - p0) +
           six * oneMinusT * tt * (p2 - p1) +
           three * tt * tt * (p3 - p2);
}
};

} // namespace Cubic

} // namespace Bezier

#endif // BEZIER_CURVES_HLSLI
