#pragma once

#include <cstdint>
#include <type_traits>

namespace solitaire {
	using u8  = uint_fast8_t;
	using u16 = uint_fast16_t;
	using u32 = uint_fast32_t;
	using u64 = uint_fast64_t;
	using s32 = int_fast32_t;

	using Rank = u8; // Card rank starts at 1 for Ace.

	template <typename E> // Scott Meyers: Effective Modern C++
	constexpr auto toUType(E enumerator) noexcept {
		return static_cast<std::underlying_type_t<E>>(enumerator);
	}
}
