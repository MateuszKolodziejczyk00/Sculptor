#ifndef RT_SPHERICAL_BASIS_HLSLI
#define RT_SPHERICAL_BASIS_HLSLI

#define RTSphericalBasisRaw float

struct RTSphericalBasis
{
	float val;

	void Encode(in float value, in float3 dir)
	{
		val = value;
	}

	float Decode(in float3 dir)
	{
		return val;
	}

	float Evaluate(in float3 dir)
	{
		return val;
	}

	RTSphericalBasis operator+(in RTSphericalBasis b)
	{
		RTSphericalBasis result;
		result.val = val + b.val;
		return result;
	}
	
	RTSphericalBasis operator*(in float b)
	{
		RTSphericalBasis result;
		result.val = val * b;
		return result;
	}

	RTSphericalBasis operator/(in float b)
	{
		RTSphericalBasis result;
		result.val = val / b;
		return result;
	}
};


RTSphericalBasis RawToRTSphericalBasis(in float basis)
{
	RTSphericalBasis output;
	output.val = basis;
	return output;
}

float RTSphericalBasisToRaw(in RTSphericalBasis basis)
{
	return isnan(basis.val) ? 0.f : basis.val;
}

RTSphericalBasis CreateRTSphericalBasis(in float value, in float3 dir)
{
	RTSphericalBasis basis;
	basis.Encode(value, dir);
	return basis;
}

float DecodeRTSphericalBasis(in RTSphericalBasis basis, in float3 dir)
{
	return basis.Decode(dir);
}

#endif // RT_SPHERICAL_BASIS_HLSLI

