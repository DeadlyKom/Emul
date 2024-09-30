#pragma once

// UnrealEngine\Engine\Source\Runtime\Core\Public\GenericPlatform.h
namespace Math
{
	// Returns true if a flag is set
	template <typename TSet, typename TFlag>
	static inline bool HasFlag(TSet set, TFlag flag)
	{
		return (set & flag) == flag;
	}

	// Set flag or flags in set
	template <typename TSet, typename TFlag>
	static inline void SetFlag(TSet& set, TFlag flags)
	{
		set = static_cast<TSet>(set | flags);
	}

	// Clear flag or flags in set
	template <typename TSet, typename TFlag>
	static inline void ClearFlag(TSet& set, TFlag flag)
	{
		set = static_cast<TSet>(set & ~flag);
	}

	// Proper modulus operator, as opposed to remainder as calculated by %
	template <typename T>
	static inline T Modulus(T a, T b)
	{
		return a - b * ImFloorSigned(a / b);
	}

	// Defined in recent versions of imgui_internal.h.  Included here in case user is on older
	// imgui version.
	static inline float ImFloorSigned(float f)
	{
		return (float)((f >= 0 || (int)f == f) ? (int)f : (int)f - 1);
	}

	static inline float Round(float f)
	{
		return ImFloorSigned(f + 0.5f);
	}

	static inline ImVec2 Abs(ImVec2 v)
	{
		return ImVec2(ImAbs(v.x), ImAbs(v.y));
	}

	template< class T >
	static inline T Max(const T A, const T B)
	{
		return (B < A) ? A : B;
	}

	template< class T >
	static inline T Min(const T A, const T B)
	{
		return (A < B) ? A : B;
	}

	template< class T >
	static inline T Clamp(const T X, const T MinValue, const T MaxValue)
	{
		return Max(Min(X, MaxValue), MinValue);
	}
}
