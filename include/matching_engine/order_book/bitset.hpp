#pragma once

#include "matching_engine/order_book/constants.hpp"

#include <array>
#include <cstdint>

template <typename T> constexpr T div_ceil(T val, T denom) {
    return (val + (denom - 1)) / denom;
}

struct PriceBitset {
private:
    static constexpr std::size_t BITSET_WORDS = div_ceil(MAX_PRICE, 64UL);
    static constexpr std::size_t L2_WORDS = div_ceil(BITSET_WORDS, 64UL);

    [[nodiscard]] constexpr std::uint64_t mask_from_lsb(std::size_t bit_idx) const noexcept;
    [[nodiscard]] constexpr std::uint64_t mask_to_msb(std::size_t bit_idx) const noexcept;

    std::array<std::uint64_t, BITSET_WORDS> l1{};
    std::array<std::uint64_t, L2_WORDS> l2{};

public:
    void set(std::size_t idx) noexcept;
    void clear(std::size_t idx) noexcept;
    void clear_all() noexcept;

    // search the first index >= `from_idx`
    // returns size_t::MAX at wrong path
    [[nodiscard]] std::size_t find_first_set_from(std::size_t from_idx) const noexcept;

    // search the first index <= `from_idx`
    // returns size_t::MAX at wrong path
    [[nodiscard]] std::size_t find_last_set_from(std::size_t from_idx) const noexcept;
};
