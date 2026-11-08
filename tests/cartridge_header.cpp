#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>

#include "core/content/cartridge_header.h"
#include "core/content/file_image.h"
#include "core/mmu/mmu.h"
#include "support/cartridge_fixture.h"
#include "support/temp_asset.h"

using gba::MMU;
using u8 = std::uint8_t;

TEST(CartridgeHeader, RecognizesFixedByteAndIndependentComplementVector) {
    constexpr auto &bytes = gba::test::kSyntheticCartridgeHeader;
    static_assert(bytes[0xB2U] == 0x96U);
    static_assert(bytes[0xBDU] == 0xBFU);

    const auto parsed = gba::parse_cartridge_header(bytes);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->title[0], 'D');
    EXPECT_EQ(parsed->title[1], 'E');
    EXPECT_EQ(parsed->title[2], 'M');
    EXPECT_EQ(parsed->title[3], 'O');
    EXPECT_EQ(parsed->game_code[0], 'A');
    EXPECT_EQ(parsed->game_code[3], 'D');
    EXPECT_EQ(parsed->maker_code[0], '0');
    EXPECT_EQ(parsed->maker_code[1], '1');
    EXPECT_EQ(parsed->software_version, 2U);
}

TEST(CartridgeHeader, RejectsMalformedHeaderWithoutReadingPastInput) {
    constexpr auto original = gba::test::kSyntheticCartridgeHeader;
    const std::array<u8, gba::CartridgeHeader::kSize - 1U> truncated{};
    EXPECT_FALSE(gba::parse_cartridge_header(truncated).has_value());

    auto badFixed = original;
    badFixed[0xB2U] = 0x00U;
    EXPECT_FALSE(gba::parse_cartridge_header(badFixed).has_value());

    auto badComplement = original;
    badComplement[0xBDU] = 0x00U;
    EXPECT_FALSE(gba::parse_cartridge_header(badComplement).has_value());

    auto badVersion = original;
    badVersion[0xBCU] = 0x03U;
    EXPECT_FALSE(gba::parse_cartridge_header(badVersion).has_value());

    EXPECT_TRUE(gba::parse_cartridge_header(original).has_value());
}

TEST(CartridgeHeader, InvalidFileReplacementPreservesExistingBytesAndMetadata) {
    const gba::test::TempAsset valid("cart-header-replacement-valid.bin");
    const gba::test::TempAsset invalid("cart-header-replacement-invalid.bin");
    ASSERT_TRUE(valid.write(gba::test::kSyntheticCartridgeHeader));

    MMU mmu;
    mmu.reset();
    gba::ImageLoadError error = gba::ImageLoadError::None;
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    ASSERT_TRUE(mmu.cartridge_header().has_value());

    auto bad = gba::test::kSyntheticCartridgeHeader;
    bad[0xB2U] = 0U;
    ASSERT_TRUE(invalid.write(bad));
    EXPECT_FALSE(mmu.load_gamepak(invalid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::InvalidHeader);
    EXPECT_FALSE(gba::image_load_error_message(error).empty());

    EXPECT_EQ(mmu.read8(MMU::WS0_BASE + 0xB2U), 0x96U);
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.cartridge_header()->title[0], 'D');

    bad = gba::test::kSyntheticCartridgeHeader;
    bad[0xBDU] ^= 0x01U;
    ASSERT_TRUE(invalid.write(bad));
    EXPECT_FALSE(mmu.load_gamepak(invalid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::InvalidHeader);
    EXPECT_EQ(mmu.read8(MMU::WS2_BASE + 0xBDU), 0xBFU);
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.cartridge_header()->title[0], 'D');

    EXPECT_FALSE(mmu.load_gamepak(std::span<const u8>{}, &error));
    EXPECT_EQ(error, gba::ImageLoadError::TooSmall);
    ASSERT_TRUE(mmu.cartridge_header().has_value());

    auto replacement = gba::test::kSyntheticCartridgeHeader;
    replacement[0xA0U] = 'E';
    replacement[0xBDU] = 0xBEU;
    ASSERT_TRUE(valid.write(replacement));
    ASSERT_TRUE(mmu.load_gamepak(valid.path(), &error));
    EXPECT_EQ(error, gba::ImageLoadError::None);
    ASSERT_TRUE(mmu.cartridge_header().has_value());
    EXPECT_EQ(mmu.cartridge_header()->title[0], 'E');
    EXPECT_EQ(mmu.read8(MMU::WS1_BASE + 0xA0U), 'E');
    EXPECT_EQ(mmu.read8(MMU::WS1_BASE + 0xBDU), 0xBEU);
}
