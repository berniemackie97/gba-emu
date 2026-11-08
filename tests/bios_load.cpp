#include <array>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>

#include "core/bus/bus.h"
#include "core/mmu/mmu.h"
#include "support/temp_asset.h"

TEST(BIOS, LoadsAndMapsFirstAndLastByte) {
    constexpr std::uint8_t kFirstByte = 0x35U;
    constexpr std::uint8_t kLastByte = 0xA6U;

    std::array<std::uint8_t, gba::MMU::BIOS_SIZE> bytes{};
    bytes.front() = kFirstByte;
    bytes.back() = kLastByte;

    const gba::test::TempAsset fixture("bios-load-valid.bin");
    ASSERT_TRUE(fixture.write(bytes));

    gba::Bus bus;
    bus.reset();
    gba::ImageLoadError error = gba::ImageLoadError::TooSmall;
    ASSERT_TRUE(bus.load_bios(fixture.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    EXPECT_EQ(bus.read8(0x0000U), kFirstByte);
    EXPECT_EQ(bus.read8(0x3FFFU), kLastByte);
}

TEST(BIOS, RejectsWrongSizesWithoutChangingPreviousImage) {
    std::array<std::uint8_t, gba::MMU::BIOS_SIZE> original{};
    original.front() = 0x42U;
    original.back() = 0x84U;
    const gba::test::TempAsset valid("bios-replace-original.bin");
    ASSERT_TRUE(valid.write(original));

    gba::Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_bios(valid.path()));

    const gba::test::TempAsset truncated("bios-replace-short.bin");
    constexpr std::array<std::uint8_t, 1> shortBytes{0xFFU};
    ASSERT_TRUE(truncated.write(shortBytes));
    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(bus.load_bios(truncated.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    const gba::test::TempAsset oversized("bios-replace-large.bin");
    std::array<std::uint8_t, gba::MMU::BIOS_SIZE + 1> longBytes{};
    ASSERT_TRUE(oversized.write(longBytes));
    EXPECT_FALSE(bus.load_bios(oversized.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooLarge);

    EXPECT_EQ(bus.read8(0x0000U), original.front());
    EXPECT_EQ(bus.read8(0x3FFFU), original.back());
    ASSERT_TRUE(bus.load_bios(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
}

TEST(BIOS, RejectsMissingAndNonRegularFiles) {
    gba::Bus bus;
    bus.reset();

    const auto missing = std::filesystem::current_path() / "nonexistent-bios-image" / "missing.bin";
    ASSERT_FALSE(std::filesystem::exists(missing));

    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(bus.load_bios(missing, &error));
    EXPECT_EQ(error, gba::ImageLoadError::Unavailable);
    EXPECT_FALSE(gba::image_load_error_message(error).empty());

    EXPECT_FALSE(bus.load_bios(std::filesystem::current_path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::NotRegularFile);
    EXPECT_EQ(bus.read8(gba::MMU::BIOS_BASE), gba::MMU::kOpenBus);
}
