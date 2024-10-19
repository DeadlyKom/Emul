#pragma once

#include <type_traits>

// UnrealEngine\Engine\Source\Runtime\Core\Public\GenericPlatform.h
#define UE_PI 							(3.1415926535897932f)	/* Extra digits if needed: 3.1415926535897932384626433832795f */
#define UE_SMALL_NUMBER					(1.e-8f)
#define UE_KINDA_SMALL_NUMBER			(1.e-4f)
#define UE_BIG_NUMBER					(3.4e+38f)
#define UE_EULERS_NUMBER				(2.71828182845904523536f)
#define UE_GOLDEN_RATIO					(1.6180339887498948482045868343656381f)	/* Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2 */
#define UE_FLOAT_NON_FRACTIONAL			(8388608.f) /* All single-precision floating point numbers greater than or equal to this have no fractional value. */


#define UE_DOUBLE_PI					(3.141592653589793238462643383279502884197169399)
#define UE_DOUBLE_SMALL_NUMBER			(1.e-8)
#define UE_DOUBLE_KINDA_SMALL_NUMBER	(1.e-4)
#define UE_DOUBLE_BIG_NUMBER			(3.4e+38)
#define UE_DOUBLE_EULERS_NUMBER			(2.7182818284590452353602874713526624977572)
#define UE_DOUBLE_GOLDEN_RATIO			(1.6180339887498948482045868343656381)	/* Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2 */
#define UE_DOUBLE_NON_FRACTIONAL		(4503599627370496.0) /* All double-precision floating point numbers greater than or equal to this have no fractional value. 2^52 */

namespace FMath
{
	// Returns true if a flag is set
	template <typename TSet, typename TFlag>
	static FORCEINLINE bool HasFlag(TSet set, TFlag flag)
	{
		return (set & flag) == flag;
	}

	// Set flag or flags in set
	template <typename TSet, typename TFlag>
	static FORCEINLINE void SetFlag(TSet& set, TFlag flags)
	{
		set = static_cast<TSet>(set | flags);
	}

	// Clear flag or flags in set
	template <typename TSet, typename TFlag>
	static FORCEINLINE void ClearFlag(TSet& set, TFlag flag)
	{
		set = static_cast<TSet>(set & ~flag);
	}

	// Proper modulus operator, as opposed to remainder as calculated by %
	template <typename T>
	static FORCEINLINE T Modulus(T a, T b)
	{
		return a - b * ImFloorSigned(a / b);
	}

	// Defined in recent versions of imgui_internal.h.  Included here in case user is on older
	// imgui version.
	static FORCEINLINE float ImFloorSigned(float f)
	{
		return (float)((f >= 0 || (int)f == f) ? (int)f : (int)f - 1);
	}

	static FORCEINLINE float Round(float f)
	{
		return ImFloorSigned(f + 0.5f);
	}

	static FORCEINLINE ImVec2 Abs(ImVec2 v)
	{
		return ImVec2(ImAbs(v.x), ImAbs(v.y));
	}

	#pragma intrinsic(_BitScanReverse64)

	static FORCEINLINE uint64_t FloorLog2_64(uint64_t Value)
	{
		unsigned long BitIndex;
		return _BitScanReverse64(&BitIndex, Value) ? BitIndex : 0;
	}

	static FORCEINLINE uint64_t CountTrailingZeros64(uint64_t Value)
	{
		// return 64 if Value is 0
		unsigned long BitIndex;	// 0-based, where the LSB is 0 and MSB is 63
		return _BitScanForward64(&BitIndex, Value) ? BitIndex : 64;
	}

	static FORCEINLINE uint32_t CountLeadingZeros(uint32_t Value)
	{
		// return 32 if value is zero
		unsigned long BitIndex;
		_BitScanReverse64(&BitIndex, uint64_t(Value) * 2 + 1);
		return 32 - BitIndex;
	}

	static FORCEINLINE uint32_t CeilLogTwo(uint32_t Arg)
	{
		// if Arg is 0, change it to 1 so that we return 0
		Arg = Arg ? Arg : 1;
		return 32 - CountLeadingZeros(Arg - 1);
	}

	static FORCEINLINE uint32_t RoundUpToPowerOfTwo(uint32_t Arg)
	{
		return 1 << CeilLogTwo(Arg);
	}

	template<typename T>
	static FORCEINLINE T Max(const T A, const T B)
	{
		return (B < A) ? A : B;
	}

	template<typename T>
	static FORCEINLINE T Min(const T A, const T B)
	{
		return (A < B) ? A : B;
	}

	template<typename T>
	static FORCEINLINE T Clamp(const T X, const T MinValue, const T MaxValue)
	{
		return Max(Min(X, MaxValue), MinValue);
	}

	static constexpr FORCEINLINE int32_t TruncToInt32(float F)
	{
		return (int32_t)F;
	}
	static constexpr FORCEINLINE int32_t TruncToInt32(double F)
	{
		return (int32_t)F;
	}
	static constexpr FORCEINLINE int64_t TruncToInt64(double F)
	{
		return (int64_t)F;
	}

	static FORCEINLINE int32_t FloorToInt32(float F)
	{
		int32_t I = TruncToInt32(F);
		I -= ((float)I > F);
		return I;
	}
	static FORCEINLINE int32_t FloorToInt32(double F)
	{
		int32_t I = TruncToInt32(F);
		I -= ((double)I > F);
		return I;
	}
	static FORCEINLINE int64_t FloorToInt64(double F)
	{
		int64_t I = TruncToInt64(F);
		I -= ((double)I > F);
		return I;
	}

	static FORCEINLINE int32_t RoundToInt32(float F)
	{
		return FloorToInt32(F + 0.5f);
	}
	static FORCEINLINE int32_t RoundToInt32(double F)
	{
		return FloorToInt32(F + 0.5);
	}
	static FORCEINLINE int64_t RoundToInt64(double F)
	{
		return FloorToInt64(F + 0.5);
	}

	static FORCEINLINE int32_t RoundToInt(float F) { return RoundToInt32(F); }
	static FORCEINLINE int64_t RoundToInt(double F) { return RoundToInt64(F); }

	template<typename T>
	FORCEINLINE T Abs(const T A)
	{
		if constexpr (std::is_same<T, float>::value)
		{
			return fabsf(A);
		}
		else if constexpr (std::is_same<T, double>::value)
		{
			return fabs(A);
		}
		return A;
	}

	static FORCEINLINE bool IsNearlyEqual(float A, float B, float ErrorTolerance = UE_SMALL_NUMBER)
	{
		return Abs<float>(A - B) <= ErrorTolerance;
	}

	static FORCEINLINE bool IsNearlyEqual(double A, double B, double ErrorTolerance = UE_DOUBLE_SMALL_NUMBER)
	{
		return Abs<double>(A - B) <= ErrorTolerance;
	}

	static FORCEINLINE bool IsNearlyZero(float Value, float ErrorTolerance = UE_SMALL_NUMBER)
	{
		return Abs<float>(Value) <= ErrorTolerance;
	}

	static FORCEINLINE bool IsNearlyZero(double Value, double ErrorTolerance = UE_DOUBLE_SMALL_NUMBER)
	{
		return Abs<double>(Value) <= ErrorTolerance;
	}
}
