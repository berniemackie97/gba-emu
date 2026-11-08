#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "core/mmu/mmu.h"
#include "support/temp_asset.h"

using gba::MMU;
using u8 = std::uint8_t;
using u32 = std::uint32_t;

namespace {
    constexpr std::array<u8, 4> kTinyRom{0xDEU, 0xADU, 0xBEU, 0xEFU};
}

TEST(GamePak, MirrorsAcrossWaitStatesAndBySize) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyRom));

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyRom[0]);
    EXPECT_EQ(mmu.read8(MMU::WS1_BASE + 1U), kTinyRom[1]);
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + 2U), kTinyRom[2]);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 4U), kTinyRom[0]);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 5U), kTinyRom[1]);
}

TEST(GamePak, WritesAreIgnored) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyRom));

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyRom[0]);
    mmu.write8(MMU::WS0_BASE, 0U);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyRom[0]);
}

TEST(GamePak, LoadsHeaderSizedFileAndRejectsTruncatedReplacement) {
    std::array<u8, MMU::GAMEPAK_HEADER_SIZE> image{};
    image.front() = 0x61U;
    image.back() = 0xB2U;
    const gba::test::TempAsset valid("gamepak-file-valid.bin");
    ASSERT_TRUE(valid.write(image));

    MMU mmu;
    mmu.reset();
    gba::ImageLoadError error = gba::ImageLoadError::TooSmall;
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), image.front());
    EXPECT_EQ(mmu.read8(MMU::WS1_BASE + static_cast<u32>(image.size() - 1U)), image.back());

    const gba::test::TempAsset shortFile("gamepak-file-short.bin");
    std::array<u8, MMU::GAMEPAK_HEADER_SIZE - 1U> truncated{};
    ASSERT_TRUE(shortFile.write(truncated));
    EXPECT_FALSE(mmu.load_gamepak(shortFile.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), image.front());
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + static_cast<u32>(image.size() - 1U)), image.back());
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
}

TEST(GamePak, RejectsEmptyOversizedAndNonRegularFiles) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyRom));

    const gba::test::TempAsset empty("gamepak-file-empty.bin");
    ASSERT_TRUE(empty.write(std::span<const u8>{}));

    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(mmu.load_gamepak(empty.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    const gba::test::TempAsset oversized("gamepak-file-oversized.bin");
    ASSERT_TRUE(oversized.write(kTinyRom));
    std::error_code resizeError;
    std::filesystem::resize_file(oversized.path(), static_cast<std::uintmax_t>(MMU::WS_REGION_SIZE_32MiB) + 1U, resizeError);
    ASSERT_FALSE(resizeError);
    EXPECT_FALSE(mmu.load_gamepak(oversized.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooLarge);

    const auto missing = std::filesystem::current_path() / "nonexistent-rom-image" / "missing.bin";
    ASSERT_FALSE(std::filesystem::exists(missing));
    EXPECT_FALSE(mmu.load_gamepak(missing, &error));
    EXPECT_EQ(error, gba::ImageLoadError::Unavailable);
    EXPECT_FALSE(mmu.load_gamepak(std::filesystem::current_path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::NotRegularFile);

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyRom[0]);
}

TEST(GamePak, MemoryImagesHaveTheSameUpperBoundAndPreserveEarlierContents) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyRom));

    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(mmu.load_gamepak(std::span<const u8>{}, &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    const std::vector<u8> oversized(static_cast<std::size_t>(MMU::WS_REGION_SIZE_32MiB) + 1U, 0xA5U);
    EXPECT_FALSE(mmu.load_gamepak(std::span<const u8>(oversized), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooLarge);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyRom[0]);

    constexpr std::array<u8, 2> replacement{0x12U, 0x34U};
    ASSERT_TRUE(mmu.load_gamepak(replacement, &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), replacement[0]);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 1U), replacement[1]);
}

TEST(GamePak, AcceptsMaximumWindowSizedFile) {
    const gba::test::TempAsset file("gamepak-file-window-limit.bin");
    std::array<u8, MMU::GAMEPAK_HEADER_SIZE> header{};
    header.front() = 0x93U;
    ASSERT_TRUE(file.write(header));

    std::error_code resizeError;
    std::filesystem::resize_file(file.path(), MMU::WS_REGION_SIZE_32MiB, resizeError);
    ASSERT_FALSE(resizeError);

    constexpr u8 kLastByte = 0xC7U;
    {
        std::fstream stream(file.path(), std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(stream.is_open());
        stream.seekp(static_cast<std::streamoff>(MMU::WS_REGION_SIZE_32MiB - 1U));
        stream.put(static_cast<char>(kLastByte));
        ASSERT_TRUE(stream.good());
    }

    MMU mmu;
    mmu.reset();
    gba::ImageLoadError error = gba::ImageLoadError::TooLarge;
    ASSERT_TRUE(mmu.load_gamepak(file.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), header.front());
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + MMU::WS_REGION_SIZE_32MiB - 1U), kLastByte);
}
