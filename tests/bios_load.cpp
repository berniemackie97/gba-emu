#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/mmu/mmu.h"

namespace {
    struct GeneratedBios {
        std::filesystem::path path;

        ~GeneratedBios() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    };
}

TEST(BIOS, LoadsAndMapsFirstAndLastByte) {
    constexpr std::uint8_t kFirstByte = 0x35U;
    constexpr std::uint8_t kLastByte = 0xA6U;

    std::array<std::uint8_t, gba::MMU::BIOS_SIZE> bytes{};
    bytes.front() = kFirstByte;
    bytes.back() = kLastByte;

    const GeneratedBios fixture{std::filesystem::current_path() / "bios_load_test.bin"};
    {
        std::ofstream output(fixture.path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(output.good());
    }

    gba::Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_bios(fixture.path));
    EXPECT_EQ(bus.read8(0x0000U), kFirstByte);
    EXPECT_EQ(bus.read8(0x3FFFU), kLastByte);
}
