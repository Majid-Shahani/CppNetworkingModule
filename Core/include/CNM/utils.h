#pragma once
#include <string_view>

namespace Carnival::utils {
	// ========================= Hash ============================ //
	constexpr uint32_t FNV32_OFFSET_BASIS{ 0x811C9DC5u };
	constexpr uint32_t FNV32_PRIME{ 0x01000193u };
	constexpr uint64_t FNV64_OFFSET_BASIS{ 0xcbf29ce484222325 };
	constexpr uint64_t FNV64_PRIME{ 0x100000001b3 };

	inline consteval uint32_t fnv1a32(std::string_view s) {
		uint32_t hash = FNV32_OFFSET_BASIS; // offset basis
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= FNV32_PRIME; // prime number
		}
		return hash;
	}

	inline consteval uint64_t fnv1a64(std::string_view s) {
		uint64_t hash = FNV64_OFFSET_BASIS;
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= FNV64_PRIME;
		}
		return hash;
	}

	// Endianness matters, hash will not be the same across different endian platforms
	inline consteval uint64_t fnv1a64(uint64_t value) {
		uint64_t hash{ FNV64_OFFSET_BASIS };
		for (uint64_t i{}; i < 8; i++) {
			uint8_t octet = (value >> (i * 8)) & 0xFF;
			hash ^= octet;
			hash *= FNV64_PRIME;
		}
		return hash;
	}
}