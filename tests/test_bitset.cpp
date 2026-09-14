#include "matching_engine/order_book/bitset.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>

constexpr std::size_t NONE_PATH = std::numeric_limits<std::size_t>::max();

TEST_CASE("Basic set and find operations", "[bitset]") {
    PriceBitset bs;

    bs.set(10);
    bs.set(100);
    bs.set(1000);

    REQUIRE(bs.find_first_set_from(0) == 10);
    REQUIRE(bs.find_first_set_from(11) == 100);
    REQUIRE(bs.find_first_set_from(101) == 1000);
    REQUIRE(bs.find_first_set_from(1001) == NONE_PATH);

    REQUIRE(bs.find_last_set_from(2000) == 1000);
    REQUIRE(bs.find_last_set_from(999) == 100);
    REQUIRE(bs.find_last_set_from(99) == 10);
    REQUIRE(bs.find_last_set_from(5) == NONE_PATH);
}

TEST_CASE("Clear and clear_all operations", "[bitset]") {
    PriceBitset bs;

    bs.set(50);
    REQUIRE(bs.find_first_set_from(0) == 50);

    bs.clear(50);
    REQUIRE(bs.find_first_set_from(0) == NONE_PATH);

    bs.set(10);
    bs.set(20);
    bs.set(30);
    bs.clear_all();

    REQUIRE(bs.find_first_set_from(0) == NONE_PATH);
    REQUIRE(bs.find_last_set_from(MAX_PRICE - 1) == NONE_PATH);
}

TEST_CASE("Word boundary transitions", "[bitset]") {
    PriceBitset bs;

    bs.set(63);
    bs.set(64);
    bs.set(127);
    bs.set(128);

    REQUIRE(bs.find_first_set_from(0) == 63);
    REQUIRE(bs.find_first_set_from(63) == 63);
    REQUIRE(bs.find_first_set_from(64) == 64);
    REQUIRE(bs.find_first_set_from(127) == 127);
    REQUIRE(bs.find_first_set_from(128) == 128);

    REQUIRE(bs.find_last_set_from(200) == 128);
    REQUIRE(bs.find_last_set_from(128) == 128);
    REQUIRE(bs.find_last_set_from(127) == 127);
    REQUIRE(bs.find_last_set_from(64) == 64);
    REQUIRE(bs.find_last_set_from(63) == 63);
}

TEST_CASE("Out of bounds handling", "[bitset]") {
    PriceBitset bs;

    bs.set(MAX_PRICE + 10);
    REQUIRE(bs.find_first_set_from(0) == NONE_PATH);

    bs.set(10);
    REQUIRE(bs.find_first_set_from(MAX_PRICE) == NONE_PATH);
}
