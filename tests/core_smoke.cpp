#include <gtest/gtest.h>
#include "core/bus/bus.h"

TEST(CoreSmoke, BusDefaults) {
    gba::Bus bus;
    bus.reset();
    EXPECT_EQ(bus.read8(0x00000000), 0xFF);
}
