// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,
// cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

#include "matching_engine/order_book/bitset.hpp"

#include <cstdint>
#include <limits>

inline constexpr std::uint64_t ALL_ONES_U64 = ~std::uint64_t{0};
inline constexpr std::size_t NONE_PATH = std::numeric_limits<std::size_t>::max();

namespace {
struct BitIndex {
    std::size_t word;
    std::size_t bit;
};

[[nodiscard]] constexpr BitIndex split_index(std::size_t idx) noexcept {
    return {.word = idx >> 6, .bit = idx & 63};
}
} // namespace

[[nodiscard]] constexpr std::uint64_t
PriceBitset::mask_from_lsb(std::size_t bit_idx) const noexcept {
    return ALL_ONES_U64 << bit_idx;
}

[[nodiscard]] constexpr std::uint64_t PriceBitset::mask_to_msb(std::size_t bit_idx) const noexcept {
    return ALL_ONES_U64 >> (63 - bit_idx);
}

void PriceBitset::set(std::size_t idx) noexcept {
    if (idx >= MAX_PRICE) [[unlikely]] {
        return;
    }

    const auto [word_idx, bit_idx] = split_index(idx);
    l1[word_idx] |= std::uint64_t{1} << bit_idx;

    const auto [l2_word_idx, l2_bit_idx] = split_index(word_idx);
    l2[l2_word_idx] |= std::uint64_t{1} << l2_bit_idx;
}

void PriceBitset::clear(std::size_t idx) noexcept {
    if (idx >= MAX_PRICE) [[unlikely]] {
        return;
    }

    const auto [word_idx, bit_idx] = split_index(idx);
    l1[word_idx] &= ~(std::uint64_t{1} << bit_idx);

    if (l1[word_idx] == 0) {
        const auto [l2_word_idx, l2_bit_idx] = split_index(word_idx);
        l2[l2_word_idx] &= ~(std::uint64_t{1} << l2_bit_idx);
    }
}

void PriceBitset::clear_all() noexcept {
    l1.fill(0);
    l2.fill(0);
}

std::size_t PriceBitset::find_first_set_from(std::size_t from_idx) const noexcept {
    if (from_idx >= MAX_PRICE) [[unlikely]] {
        return NONE_PATH;
    }

    auto [word_idx, bit_idx] = split_index(from_idx);

    const auto masked = l1[word_idx] & mask_from_lsb(bit_idx);
    if (masked != 0) {
        const auto res = (word_idx << 6) + static_cast<std::size_t>(std::countr_zero(masked));
        return res < MAX_PRICE ? res : NONE_PATH;
    }

    word_idx += 1;
    if (word_idx >= BITSET_WORDS) {
        return NONE_PATH;
    }

    auto [l2_word_idx, l2_bit_idx] = split_index(word_idx);
    const auto l2_masked = l2[l2_word_idx] & mask_from_lsb(l2_bit_idx);

    if (l2_masked != 0) {
        word_idx = (l2_word_idx << 6) + static_cast<std::size_t>(std::countr_zero(l2_masked));
        const auto res = (word_idx << 6) + static_cast<std::size_t>(std::countr_zero(l1[word_idx]));
        return res < MAX_PRICE ? res : NONE_PATH;
    }

    l2_word_idx += 1;
    while (l2_word_idx < L2_WORDS) {
        const auto l2_val = l2[l2_word_idx];
        if (l2_val != 0) {
            word_idx = (l2_word_idx << 6) + static_cast<std::size_t>(std::countr_zero(l2_val));
            const auto res =
                (word_idx << 6) + static_cast<std::size_t>(std::countr_zero(l1[word_idx]));
            return res < MAX_PRICE ? res : NONE_PATH;
        }
        l2_word_idx += 1;
    }

    return NONE_PATH;
}

std::size_t PriceBitset::find_last_set_from(std::size_t from_idx) const noexcept {
    const auto min_from_idx = std::min(from_idx, MAX_PRICE - 1);

    auto [word_idx, bit_idx] = split_index(min_from_idx);

    const auto masked = l1[word_idx] & mask_to_msb(bit_idx);
    if (masked != 0) {
        return (word_idx << 6) + (63 - static_cast<std::size_t>(std::countl_zero(masked)));
    }

    if (word_idx == 0) {
        return NONE_PATH;
    }
    word_idx -= 1;

    auto [l2_word_idx, l2_bit_idx] = split_index(word_idx);

    const auto l2_masked = l2[l2_word_idx] & mask_to_msb(l2_bit_idx);

    if (l2_masked != 0) {
        word_idx =
            (l2_word_idx << 6) + (63 - static_cast<std::size_t>(std::countl_zero(l2_masked)));
        return (word_idx << 6) + (63 - static_cast<std::size_t>(std::countl_zero(l1[word_idx])));
    }

    while (l2_word_idx > 0) {
        l2_word_idx -= 1;
        const auto l2_val = l2[l2_word_idx];
        if (l2_val != 0) {
            word_idx = (l2_word_idx << 6) + 63 - static_cast<std::size_t>(std::countl_zero(l2_val));
            return (word_idx << 6) + 63 - static_cast<std::size_t>(std::countl_zero(l1[word_idx]));
        }
    }

    return NONE_PATH;
}

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,
// cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
