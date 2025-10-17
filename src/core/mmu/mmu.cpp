// src/core/mmu/mmu.cpp
#include "core/mmu/mmu.h"
#include <cstdint>
#include <fstream>
#include <vector>

using namespace gba;

auto MMU::reset() noexcept -> void {
    bios_.fill(0x00);
    bios_loaded_ = false;
}

auto MMU::load_bios(const std::filesystem::path &file) noexcept -> bool {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        return false;
    }
    std::vector<unsigned char> buf(std::istreambuf_iterator<char>(ifs), {});
    if (buf.size() < BIOS_SIZE) { // must be full 16 KiB
        return false;
    }
    std::copy_n(buf.data(), BIOS_SIZE, bios_.data()); // ignore any extra bytes
    bios_loaded_ = true;
    return true;
}

auto MMU::read8(std::uint32_t addr) const noexcept -> std::uint8_t {
    // BIOS region 0x0000_0000 - 0x0000_3FFF
    if (addr < BIOS_SIZE && bios_loaded_) {
        return bios_.at(static_cast<std::size_t>(addr));
    }
    return kOpenBus;
}
