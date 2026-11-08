#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "core/mmu/mmu.h"
#include "support/cartridge_fixture.h"
#include "support/temp_asset.h"

using gba::MMU;
using u8 = std::uint8_t;
using u32 = std::uint32_t;

namespace {
    constexpr std::array<u8, 4> kTinyProgram{0xDEU, 0xADU, 0xBEU, 0xEFU};
}

TEST(GamePak, WaitStateWindowsShareDirectOffsetsWithoutLengthModulo) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyProgram));

    for (const u32 base : {MMU::WS0_BASE, MMU::WS1_BASE, MMU::WS2_BASE}) {
        EXPECT_EQ(mmu.read8(base), kTinyProgram.front());
        EXPECT_EQ(mmu.read8(base + 2U), kTinyProgram[2]);
        EXPECT_EQ(mmu.read8(base + 3U), kTinyProgram.back());
        EXPECT_EQ(mmu.read8(base + 4U), MMU::kOpenBus);
        EXPECT_EQ(mmu.read8(base + 6U), MMU::kOpenBus);
        EXPECT_EQ(mmu.read8(base + MMU::WS_REGION_SIZE_32MiB - 1U), MMU::kOpenBus);
    }

    mmu.write8(MMU::WS0_BASE, 0U);
    mmu.write16(MMU::WS1_BASE + 2U, 0U);
    mmu.write32(MMU::WS2_BASE, 0U);
    EXPECT_EQ(mmu.read32(MMU::WS0_BASE), 0xEFBEADDEU);
}

TEST(GamePak, HeaderSizedFileLoadsAndTruncatedReplacementIsRejected) {
    auto image = gba::test::kSyntheticCartridgeHeader;
    image.back() = 0xB2U;
    const gba::test::TempAsset valid("gamepak-file-valid.bin");
    ASSERT_TRUE(valid.write(image));

    MMU mmu;
    mmu.reset();
    gba::ImageLoadError error = gba::ImageLoadError::TooSmall;
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.cartridge_header()->software_version, 2U);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), image.front());
    EXPECT_EQ(mmu.read8(MMU::WS1_BASE + static_cast<u32>(image.size() - 1U)), image.back());

    const gba::test::TempAsset shortFile("gamepak-file-short.bin");
    std::array<u8, MMU::GAMEPAK_HEADER_SIZE - 1U> truncated{};
    ASSERT_TRUE(shortFile.write(truncated));
    EXPECT_FALSE(mmu.load_gamepak(shortFile.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), image.front());
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + static_cast<u32>(image.size() - 1U)), image.back());
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
}

TEST(GamePak, RejectsEmptyOversizedAndNonRegularFiles) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyProgram));

    const gba::test::TempAsset empty("gamepak-file-empty.bin");
    ASSERT_TRUE(empty.write(std::span<const u8>{}));

    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(mmu.load_gamepak(empty.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    const gba::test::TempAsset oversized("gamepak-file-oversized.bin");
    ASSERT_TRUE(oversized.write(kTinyProgram));
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

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyProgram.front());
}

TEST(GamePak, MemoryImagesRemainBoundedAndRecoverAfterInvalidInput) {
    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(kTinyProgram));

    gba::ImageLoadError error = gba::ImageLoadError::None;
    EXPECT_FALSE(mmu.load_gamepak(std::span<const u8>{}, &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);

    const std::vector<u8> oversized(static_cast<std::size_t>(MMU::WS_REGION_SIZE_32MiB) + 1U, 0xA5U);
    EXPECT_FALSE(mmu.load_gamepak(std::span<const u8>(oversized), &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooLarge);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyProgram.front());

    constexpr std::array<u8, 2> replacement{0x12U, 0x34U};
    ASSERT_TRUE(mmu.load_gamepak(replacement, &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    EXPECT_FALSE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), replacement[0]);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 1U), replacement[1]);
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 2U), MMU::kOpenBus);
}

TEST(GamePak, NonPowerOfTwoFilePreservesBytesWithoutModuloAliases) {
    std::vector<u8> image(gba::test::kSyntheticCartridgeHeader.begin(),
                          gba::test::kSyntheticCartridgeHeader.end());
    image.resize(257U, 0x37U);
    image.back() = 0xE3U;

    const gba::test::TempAsset fixture("gamepak-file-nonpower.bin");
    ASSERT_TRUE(fixture.write(image));

    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(fixture.path()));

    for (const u32 base : {MMU::WS0_BASE, MMU::WS1_BASE, MMU::WS2_BASE}) {
        EXPECT_EQ(mmu.read8(base + 256U), 0xE3U);
        EXPECT_EQ(mmu.read8(base + 257U), MMU::kOpenBus);
        EXPECT_EQ(mmu.read8(base + 514U), MMU::kOpenBus);
        EXPECT_EQ(mmu.read8(base + MMU::WS_REGION_SIZE_32MiB - 1U), MMU::kOpenBus);
    }
}

TEST(GamePak, MaximumWindowSizedFileIncludesLastByte) {
    const gba::test::TempAsset file("gamepak-file-window-limit.bin");
    ASSERT_TRUE(file.write(gba::test::kSyntheticCartridgeHeader));

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
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), gba::test::kSyntheticCartridgeHeader.front());
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + MMU::WS_REGION_SIZE_32MiB - 1U), kLastByte);
}

TEST(GamePak, LoadedMetadataIsClearedOnResetAndRawImageReplacement) {
    const gba::test::TempAsset file("gamepak-file-metadata-lifecycle.bin");
    ASSERT_TRUE(file.write(gba::test::kSyntheticCartridgeHeader));

    MMU mmu;
    mmu.reset();
    ASSERT_TRUE(mmu.load_gamepak(file.path()));
    ASSERT_TRUE(mmu.cartridge_header().has_value());

    ASSERT_TRUE(mmu.load_gamepak(kTinyProgram));
    EXPECT_FALSE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), kTinyProgram.front());

    ASSERT_TRUE(mmu.load_gamepak(file.path()));
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    mmu.reset();
    EXPECT_FALSE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.read8(MMU::WS0_BASE), MMU::kOpenBus);
}
