#ifndef SPHERICAL_HARMONICS_HLSLI
#define SPHERICAL_HARMONICS_HLSLI

template<typename T>
struct SH2
{
	T coeffs[4];

	T operator[](in int index)
	{
		return coeffs[index];
	}

	T Evaluate(in float3 dir)
	{
		return (T)(coeffs[0] + coeffs[1] * dir.y + coeffs[2] * dir.z + coeffs[3] * dir.x);
	}

	SH2<T> operator+(in SH2<T> b)
	{
		SH2<T> result;
		result.coeffs[0] = coeffs[0] + b.coeffs[0];
		result.coeffs[1] = coeffs[1] + b.coeffs[1];
		result.coeffs[2] = coeffs[2] + b.coeffs[2];
		result.coeffs[3] = coeffs[3] + b.coeffs[3];
		return result;
	}
	
	
	SH2<T> operator*(in T b)
	{
		SH2<T> result;
		result.coeffs[0] = coeffs[0] * b;
		result.coeffs[1] = coeffs[1] * b;
		result.coeffs[2] = coeffs[2] * b;
		result.coeffs[3] = coeffs[3] * b;
		return result;
	}

	SH2<T> operator/(in T b)
	{
		SH2<T> result;
		result.coeffs[0] = coeffs[0] / b;
		result.coeffs[1] = coeffs[1] / b;
		result.coeffs[2] = coeffs[2] / b;
		result.coeffs[3] = coeffs[3] / b;
		return result;
	}
};


template<typename T>
SH2<T> CreateSH2(in T data, in float3 dir)
{
	SH2<T> result;
	result.coeffs[0] = T(data * 0.282095f); // 1/2 * sqrt(1/pi)
	result.coeffs[1] = T(data * 0.488603f * dir.y); // sqrt(3/(4*pi)) * y
	result.coeffs[2] = T(data * 0.488603f * dir.z); // sqrt(3/(4*pi)) * z
	result.coeffs[3] = T(data * 0.488603f * dir.x); // sqrt(3/(4*pi)) * x
	return result;
}


half4 SH2ToHalf4(in SH2<half> sh)
{
	return half4(sh.coeffs[0], sh.coeffs[1], sh.coeffs[2], sh.coeffs[3]);
}


SH2<half> Half4ToSH2(in half4 data)
{
	SH2<half> result;
	result.coeffs[0] = data.x;
	result.coeffs[1] = data.y;
	result.coeffs[2] = data.z;
	result.coeffs[3] = data.w;
	return result;
}


float4 SH2ToFloat4(in SH2<float> sh)
{
	return float4(sh.coeffs[0], sh.coeffs[1], sh.coeffs[2], sh.coeffs[3]);
}


SH2<float> Float4ToSH2(in float4 data)
{
	SH2<float> result;
	result.coeffs[0] = data.x;
	result.coeffs[1] = data.y;
	result.coeffs[2] = data.z;
	result.coeffs[3] = data.w;
	return result;
}


SH2<float> SH2HalfToFloat(in SH2<half> data)
{
	return Float4ToSH2(SH2ToHalf4(data));
}


#endif // SPHERICAL_HARMONICS_HLSLI