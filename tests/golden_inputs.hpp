// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <vector>

// std::shuffle output is implementation-defined (libc++ vs libstdc++).
// This is the libc++ sequence for seed 0x474f4c44454e and size 47.
inline std::vector<std::uint32_t> golden_random_47() {
    return {
        1, 18, 34, 17, 27, 33, 46, 13, 28, 36, 10, 31, 39, 26, 8, 35, 30, 38,
        19, 44, 23, 11, 21, 42, 7, 6, 20, 29, 16, 37, 3, 25, 15, 5, 43, 12,
        45, 24, 22, 9, 32, 4, 2, 41, 40, 14, 0};
}
