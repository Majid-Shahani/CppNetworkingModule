#pragma once
#include<print>
#ifdef CL_ENABLE_ASSERTS
	#define CL_CORE_ASSERT(x, ...) \
		do { \
			if(!(x)) { \
				std::print("Assertion Failed: "); \
				std::print(__VA_ARGS__); \
				__debugbreak(); \
			} \
		} while(0)
#else
	#define CL_CORE_ASSERT(x, ...)
#endif
