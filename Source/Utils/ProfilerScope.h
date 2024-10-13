#pragma once

#include <CoreMinimal.h>

struct FProfilerScope
{
	FProfilerScope(int32_t Time, std::function<bool()>&& Condition)
	{
		Container.Time = Time;
		Container.Condition = std::move(Condition);
	}

	template<typename F>
	auto operator+(F&& Func)
	{
		if (Container.Time > 0)
		{
			Container.Counter = 0;
			Container.StartTime = std::chrono::system_clock::now();
			while (++Container.Counter < Container.Time) Func();
		}
		else
		{
			while (Container.Condition()) Func();
		}
		return FScope(Container);
	}

	struct FContainer
	{
		int32_t Time;
		int32_t Counter;
		std::function<bool()> Condition;
		std::chrono::system_clock::time_point StartTime;
	};

	FContainer Container;

	struct FScope
	{
		FContainer Container;

		~FScope()
		{
			if (Container.Time > 0)
			{
				std::chrono::duration<int64_t, std::nano> ElapsedTime = std::chrono::system_clock::now() - Container.StartTime;
				double TimeDurationSec = ElapsedTime.count() / 1000000000.0;
				LOG("--------------------------------"); 
				LOG("[{}] : {}", Container.Counter, TimeDurationSec);
				LOG("--------------------------------\n");
			}
		}
	};
};

#define PRIVATE_PROFILER_SCOPE_JOIN(A, B) PRIVATE_PROFILER_SCOPE_JOIN_INNER(A, B)
#define PRIVATE_PROFILER_SCOPE_JOIN_INNER(A, B) A##B

#define PROFILER_SCOPE(...) \
	const auto PRIVATE_PROFILER_SCOPE_JOIN(ProfilerScope_, __LINE__) = FProfilerScope(__VA_ARGS__) + [&]
