// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cstdint>

namespace benes::test {

// Fixed input for random-47.benes. std::shuffle is not reproducible across
// standard libraries, so the golden fixture must use explicit values.
inline constexpr std::array<std::uint32_t, 47> random47{
    1, 18, 34, 17, 27, 33, 46, 13, 28, 36, 10, 31,
    39, 26, 8, 35, 30, 38, 19, 44, 23, 11, 21, 42,
    7, 6, 20, 29, 16, 37, 3, 25, 15, 5, 43, 12,
    45, 24, 22, 9, 32, 4, 2, 41, 40, 14, 0,
};

} // namespace benes::test
