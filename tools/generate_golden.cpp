// SPDX-License-Identifier: Apache-2.0
#include <benes/benes.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

namespace {

void write(
    const std::filesystem::path& directory,
    const std::string& name,
    std::span<const std::uint32_t> permutation) {
    const auto bytes = benes::compress(permutation,
        {.backend = benes::Backend::portable_cpu, .threads = 1});
    std::ofstream output(directory / name, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    std::vector<std::uint32_t> identity(32);
    std::iota(identity.begin(), identity.end(), 0);
    write(directory, "identity-32.benes", identity);

    std::vector<std::uint32_t> reverse(33);
    std::iota(reverse.rbegin(), reverse.rend(), 0);
    write(directory, "reverse-33.benes", reverse);

    // std::shuffle output is implementation-defined (libc++ vs libstdc++).
    const std::vector<std::uint32_t> random{
        1, 18, 34, 17, 27, 33, 46, 13, 28, 36, 10, 31, 39, 26, 8, 35, 30, 38,
        19, 44, 23, 11, 21, 42, 7, 6, 20, 29, 16, 37, 3, 25, 15, 5, 43, 12,
        45, 24, 22, 9, 32, 4, 2, 41, 40, 14, 0};
    write(directory, "random-47.benes", random);
    return 0;
}
