#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace bench::common::itch {

constexpr std::size_t LENGTH_PREFIX_BYTES = 2;

inline std::vector<std::uint8_t> load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << path << '\n';
        std::exit(EXIT_FAILURE);
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << path << '\n';
        std::exit(EXIT_FAILURE);
    }
    return buffer;
}

class Splitter {
public:
    explicit Splitter(std::span<const std::uint8_t> buf) : buf_(buf) {}

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::span<const std::uint8_t>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        Iterator(std::span<const std::uint8_t> buf, std::size_t pos) : buf_(buf), pos_(pos) {
            advance_to_valid();
        }

        value_type operator*() const { return current_msg_; }

        Iterator& operator++() {
            pos_ += LENGTH_PREFIX_BYTES + current_msg_.size();
            advance_to_valid();
            return *this;
        }

        bool operator==(const Iterator& other) const { return pos_ == other.pos_; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        void advance_to_valid() {
            if (pos_ + LENGTH_PREFIX_BYTES > buf_.size()) {
                pos_ = buf_.size();
                return;
            }
            const std::uint16_t len = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(buf_[pos_]) << 8) | buf_[pos_ + 1]);
            if (len == 0 || pos_ + LENGTH_PREFIX_BYTES + len > buf_.size()) {
                pos_ = buf_.size();
                return;
            }
            current_msg_ = buf_.subspan(pos_ + LENGTH_PREFIX_BYTES, len);
        }

        std::span<const std::uint8_t> buf_;
        std::size_t pos_;
        std::span<const std::uint8_t> current_msg_;
    };

    [[nodiscard]] Iterator begin() const { return {buf_, pos_}; }
    [[nodiscard]] Iterator end() const { return {buf_, buf_.size()}; }

private:
    std::span<const std::uint8_t> buf_;
    std::size_t pos_{};
};

inline void normalize_locate(std::span<std::uint8_t> buf, std::uint16_t locate) {
    std::size_t pos = 0;
    const std::uint8_t loc_bytes[2] = {static_cast<std::uint8_t>(locate >> 8),
                                       static_cast<std::uint8_t>(locate & 0xFF)};

    while (pos + LENGTH_PREFIX_BYTES <= buf.size()) {
        const std::uint16_t len = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(buf[pos]) << 8) | buf[pos + 1]);
        if (len == 0 || pos + LENGTH_PREFIX_BYTES + len > buf.size()) {
            return;
        }
        buf[pos + LENGTH_PREFIX_BYTES + 1] = loc_bytes[0];
        buf[pos + LENGTH_PREFIX_BYTES + 2] = loc_bytes[1];
        pos += LENGTH_PREFIX_BYTES + len;
    }
}

inline std::size_t count_messages(std::span<const std::uint8_t> buf) {
    std::size_t count = 0;
    for ([[maybe_unused]] auto msg : Splitter(buf)) {
        ++count;
    }
    return count;
}

inline std::vector<std::size_t> distinct_locates(std::span<const std::uint8_t> buf) {
    std::unordered_set<std::size_t> seen;
    std::vector<std::size_t> out;

    for (auto msg : Splitter(buf)) {
        if (msg.empty()) {
            continue;
        }
        const std::uint8_t type = msg[0];
        if (type != 'A' && type != 'F' && type != 'C' && type != 'E' && type != 'D' &&
            type != 'X' && type != 'U' && type != 'S') {
            continue;
        }
        const std::size_t locate =
            (static_cast<std::size_t>(msg[1]) << 8) | static_cast<std::size_t>(msg[2]);
        if (seen.insert(locate).second) {
            out.push_back(locate);
        }
    }
    return out;
}

} // namespace bench::common::itch
