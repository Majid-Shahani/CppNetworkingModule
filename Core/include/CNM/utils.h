#pragma once

namespace Carnival::utils {
	inline consteval uint32_t fnv1a32(std::string_view s) {
		uint32_t hash = 0x811C9DC5u; // offset
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= 0x01000193u; // prime number
		}
		return hash;
	}
}