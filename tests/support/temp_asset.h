#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <system_error>

namespace gba::test {

class TempAsset {
  public:
    explicit TempAsset(std::string_view filename) : path_(std::filesystem::current_path() / filename) {}
    TempAsset(const TempAsset &) = delete;
    auto operator=(const TempAsset &) -> TempAsset & = delete;

    ~TempAsset() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return path_; }

    [[nodiscard]] auto write(std::span<const std::uint8_t> bytes) const -> bool {
        std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        if (!bytes.empty()) {
            stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        stream.flush();
        return stream.good();
    }

  private:
    std::filesystem::path path_;
};

} // namespace gba::test
