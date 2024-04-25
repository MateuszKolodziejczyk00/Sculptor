#pragma once

#include "MathCore.h"
#include <concepts>


namespace spt::math
{

struct Utils
{
public:

	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType RoundUp(TType value, TType multiple)
	{
		return ((value + multiple - 1) / multiple) * multiple;
	}

	template<typename TType> requires std::is_integral_v<TType>
	static constexpr Bool IsPowerOf2(TType value)
	{
		return (value & (value - 1)) == 0;
	}

	/** Based on Toby Speight answer: https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2 */
	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType RoundUpToPowerOf2(TType value)
	{
		--value;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		if constexpr (sizeof(TType) > 32)
		{
			value |= value >> 32;
		}
		if constexpr (sizeof(TType) > 64)
		{
			value |= value >> 64;
		}
		++value;

		return value;
	}
	
	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType PreviousPowerOf2(TType value)
	{
		return RoundUpToPowerOf2(value) >> 1;
	}

	
	/** Based on Paul R answer: https://stackoverflow.com/questions/2679815/previous-power-of-2 */
	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType RoundDownToPowerOf2(TType value)
	{
		value = value | (value >> 1);
		value = value | (value >> 2);
		value = value | (value >> 4);
		value = value | (value >> 8);
		value = value | (value >> 16);
		if constexpr (sizeof(TType) > 32)
		{
			value = value | (value >> 32);
		}
		if constexpr (sizeof(TType) > 64)
		{
			value = value | (value >> 64);
		}
		return value - (value >> 1);
	}

	template<typename TType> requires std::is_floating_point_v<TType>
	static constexpr TType DegreesToRadians(TType valueInDegees)
	{
		constexpr TType degreesToRadiansMultiplier = Pi<TType> / static_cast<TType>(180.0);
		return valueInDegees * degreesToRadiansMultiplier;
	}

	template<typename TType> requires std::is_floating_point_v<TType>
	static constexpr TType RadiansToDegrees(TType valueInRadians)
	{
		constexpr TType radiansToDegreesMultiplier =  static_cast<TType>(180.0) / Pi<TType>;
		return valueInRadians * radiansToDegreesMultiplier;
	}

	template<typename TType>
	static TType Square(TType val)
	{
		return val * val;
	}

	template<typename TType> requires std::is_floating_point_v<TType>
	static constexpr Bool IsNearlyZero(TType value, TType epsilon = smallNumber<TType>)
	{
		return std::abs(value) < epsilon;
	}

	template<typename TType> requires std::is_floating_point_v<TType>
	static constexpr Bool AreNearlyEqual(TType v1, TType v2, TType epsilon = smallNumber<TType>)
	{
		return IsNearlyZero(v1 - v2, epsilon);
	}

	template<typename TType>
	static constexpr TType FirstSetBit(TType value)
	{
		return value & -value;
	}

	template<std::unsigned_integral TType>
	static constexpr TType LowestSetBitIdx(TType value)
	{
		return std::countr_zero(value);
	}

	template<std::integral TType>
	static constexpr Uint32 CountSetBits(TType value)
	{
		Uint32 count = 0;

		while (value != 0)
		{
			count += (value & 1);
			value >>= 1;
		}

		return count;
	}

	template<std::unsigned_integral TType>
	static constexpr TType DivideCeil(TType numerator, TType denominator)
	{
		return ((numerator - 1) / denominator) + 1;
	}

	template<std::unsigned_integral TType, int rows, int cols>
	static constexpr math::Matrix<TType, rows, cols> DivideCeil(const math::Matrix<TType, rows, cols>& numerator, const math::Matrix<TType, rows, cols>& denominator)
	{
		math::Matrix<TType, rows, cols> result;
		
		for (int row = 0; row < rows; ++row)
		{
			for (int col = 0; col < cols; ++col)
			{
				result(row, col) = DivideCeil(numerator.coeff(row, col), denominator.coeff(row, col));
			}
		}

		return result;;
	}

	template<std::floating_point TType>
	static TType FractionalPart(TType value)
	{
		return value - std::floor(value);
	}

	static Uint32 ComputeMipLevelsNumForResolution(math::Vector2u resolution)
	{
		return 1u + static_cast<Uint32>(std::log2(std::max(resolution.x(), resolution.y())));
	}

	static math::Vector3u ComputeMipResolution(const math::Vector3u& resolution, Uint32 mipLevel)
	{
		math::Vector3u mipResolution{};
		mipResolution.x() = std::max<Uint32>(resolution.x() >> mipLevel, 1);
		mipResolution.y() = std::max<Uint32>(resolution.y() >> mipLevel, 1);
		mipResolution.z() = std::max<Uint32>(resolution.z() >> mipLevel, 1);

		return mipResolution;
	}

	static Quaternionf EulerToQuaternionRadians(Real32 roll, Real32 pitch, Real32 yaw)
	{
		return AngleAxisf(roll, Vector3f::UnitX()) * AngleAxisf(pitch, Vector3f::UnitY()) * AngleAxisf(yaw, Vector3f::UnitZ());
	}

	static Quaternionf EulerToQuaternionDegrees(Real32 roll, Real32 pitch, Real32 yaw)
	{
		return EulerToQuaternionRadians(DegreesToRadians(roll), DegreesToRadians(pitch), DegreesToRadians(yaw));
	}

	static Matrix3f EulerToRotationMatrixRadians(Real32 roll, Real32 pitch, Real32 yaw)
	{
		return EulerToQuaternionRadians(roll, pitch, yaw).matrix();
	}

	static Matrix3f EulerToRotationMatrixDegrees(Real32 roll, Real32 pitch, Real32 yaw)
	{
		return EulerToRotationMatrixRadians(DegreesToRadians(roll), DegreesToRadians(pitch), DegreesToRadians(yaw));
	}
};

} // spt::math