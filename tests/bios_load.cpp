// tests/bios_load.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include "core/bus/bus.h"

TEST(BIOS, LoadsAndMapsFirstAndLastByte) {
    using std::filesystem::path;
    const auto bios = path(PROJECT_SOURCE_DIR) / "assets" / "gba_bios.bin";
    ASSERT_TRUE(std::filesystem::exists(bios)) << bios.string();

    std::ifstream ifs(bios, std::ios::binary);
    std::vector<unsigned char> buf(std::istreambuf_iterator<char>(ifs), {});
    ASSERT_GE(buf.size(), 0x4000u);

    gba::Bus bus;
    bus.reset();
    ASSERT_TRUE(bus.load_bios(bios));

    EXPECT_EQ(bus.read8(0x0000), buf[0x0000]);
    EXPECT_EQ(bus.read8(0x3FFF), buf[0x3FFF]);
}
