#pragma once
#include <string_view>

namespace Carnival::utils {
	inline consteval uint32_t fnv1a32(std::string_view s) {
		uint32_t hash = 0x811C9DC5u; // offset basis
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= 0x01000193u; // prime number
		}
		return hash;
	}
	inline consteval uint64_t fnv1a64(std::string_view s) {
		uint64_t hash{ 0xcbf29ce484222325 };
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= 0x100000001b3;
		}
		return hash;
	}
}