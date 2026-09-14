#include "matching_engine/itch_parser/messages.hpp"
#include "matching_engine/order_book/book.hpp"

#include <array>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
namespace {
template <typename T> constexpr T to_be(T val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return val;
    } else {
        return std::byteswap(val);
    }
}

template <typename T, std::size_t N>
inline void write_be(std::array<std::uint8_t, N>& buf, std::size_t offset, T val) noexcept {
    T be_val = to_be(val);
    std::memcpy(buf.data() + offset, &be_val, sizeof(T));
}

template <std::size_t N>
inline void write_ts48(std::array<std::uint8_t, N>& buf, std::size_t offset,
                       std::uint64_t ts) noexcept {
    std::uint64_t be_ts = to_be(ts);
    std::memcpy(buf.data() + offset, reinterpret_cast<const std::uint8_t*>(&be_ts) + 2, 6);
}

inline std::array<std::uint8_t, 36> build_add_order_no_mpid(std::uint64_t order_ref_number,
                                                            std::uint8_t indicator,
                                                            std::uint32_t shares,
                                                            std::uint32_t price,
                                                            std::uint64_t timestamp) noexcept {
    std::array<std::uint8_t, 36> buf{};
    buf[0] = 'A';
    write_be(buf, 1, static_cast<std::uint16_t>(100));
    write_be(buf, 3, static_cast<std::uint16_t>(200));
    write_ts48(buf, 5, timestamp);
    write_be(buf, 11, order_ref_number);
    buf[19] = indicator;
    write_be(buf, 20, shares);
    std::memcpy(buf.data() + 24, "AAPL    ", 8);
    write_be(buf, 32, price);
    return buf;
}

inline std::array<std::uint8_t, 40>
build_add_order_with_mpid(std::uint64_t order_ref_number, std::uint8_t indicator,
                          std::uint32_t shares, std::uint32_t price, std::uint64_t timestamp,
                          std::uint32_t attribution) noexcept {
    std::array<std::uint8_t, 40> buf{};
    buf[0] = 'F';
    write_be(buf, 1, static_cast<std::uint16_t>(101));
    write_be(buf, 3, static_cast<std::uint16_t>(201));
    write_ts48(buf, 5, timestamp);
    write_be(buf, 11, order_ref_number);
    buf[19] = indicator;
    write_be(buf, 20, shares);
    std::memcpy(buf.data() + 24, "GOOGL   ", 8);
    write_be(buf, 32, price);
    write_be(buf, 36, attribution);
    return buf;
}

inline std::array<std::uint8_t, 35> build_order_replace(std::uint64_t org_order_ref_number,
                                                        std::uint64_t new_order_ref_number,
                                                        std::uint32_t shares, std::uint32_t price,
                                                        std::uint64_t timestamp) noexcept {
    std::array<std::uint8_t, 35> buf{};
    buf[0] = 'U';
    write_be(buf, 1, static_cast<std::uint16_t>(100));
    write_be(buf, 3, static_cast<std::uint16_t>(200));
    write_ts48(buf, 5, timestamp);
    write_be(buf, 11, org_order_ref_number);
    write_be(buf, 19, new_order_ref_number);
    write_be(buf, 27, shares);
    write_be(buf, 31, price);
    return buf;
}

inline std::array<std::uint8_t, 12> build_system_event(std::uint8_t event_code,
                                                       std::uint64_t timestamp) noexcept {
    std::array<std::uint8_t, 12> buf{};
    buf[0] = 'S';
    write_be(buf, 1, static_cast<std::uint16_t>(100));
    write_be(buf, 3, static_cast<std::uint16_t>(200));
    write_ts48(buf, 5, timestamp);
    buf[11] = event_code;
    return buf;
}

inline std::vector<Id> level_ids(const OrderBook& book, Side side, std::uint32_t price) {
    const PriceLevel* level = book.get_price_level(side, Price{price});
    if (level == nullptr) {
        return {};
    }

    std::vector<Id> ids;
    ids.reserve(level->count);

    auto idx = level->head;
    while (!idx.is_null()) {
        ids.push_back(book.order_pool_get(idx).id);
        idx = book.order_pool_get(idx).next;
    }
    return ids;
}

inline std::uint64_t best_bid_price(const OrderBook& book) noexcept {
    const std::uint64_t offset = book.price_offset();
    return (static_cast<std::uint64_t>(book.best_bid()) + offset) * TICK_SIZE;
}

inline std::uint64_t best_ask_price(const OrderBook& book) noexcept {
    const std::uint64_t offset = book.price_offset();
    return (static_cast<std::uint64_t>(book.best_ask()) + offset) * TICK_SIZE;
}
} // namespace

// NOLINTEND(bugprone-easily-swappable-parameters)

// ---- on_add_order_no_mpid ----

TEST_CASE("add buy order no mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID = 42;
    constexpr std::uint8_t SIDE = 'B';
    constexpr std::uint32_t SHARES = 100;
    constexpr std::uint32_t PRICE = 15000;
    constexpr std::uint64_t TS = 1'000'000;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, SIDE, SHARES, PRICE, TS);
    const auto* msg = AddOrderNoMPID::new_msg(buf);
    REQUIRE(msg != nullptr);

    const auto slab = ob.add_order_no_mpid(*msg);
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == SHARES);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == SHARES);
    REQUIRE(order.side == Side::BUY);
    REQUIRE(order.price.value == PRICE);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

TEST_CASE("add sell order no mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID = 0;
    constexpr std::uint8_t SIDE = 'S';
    constexpr std::uint32_t SHARES = 200;
    constexpr std::uint32_t PRICE = 25000;
    constexpr std::uint64_t TS = 2'000'000;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, SIDE, SHARES, PRICE, TS);
    const auto* msg = AddOrderNoMPID::new_msg(buf);
    REQUIRE(msg != nullptr);

    const auto slab = ob.add_order_no_mpid(*msg);
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == SHARES);
    REQUIRE(level_ids(ob, Side::SELL, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == SHARES);
    REQUIRE(order.side == Side::SELL);
    REQUIRE(order.price.value == PRICE);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("multiple buy orders same price no mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID_1 = 1;
    constexpr std::uint64_t ID_2 = 2;
    constexpr std::uint8_t SIDE = 'B';
    constexpr std::uint32_t SHARES_1 = 50;
    constexpr std::uint32_t SHARES_2 = 75;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint64_t TS_1 = 100;
    constexpr std::uint64_t TS_2 = 200;

    auto ob = OrderBook();
    const auto buf1 = build_add_order_no_mpid(ID_1, SIDE, SHARES_1, PRICE, TS_1);
    const auto buf2 = build_add_order_no_mpid(ID_2, SIDE, SHARES_2, PRICE, TS_2);

    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf1));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf2));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 2);
    REQUIRE(level->volume.value == SHARES_1 + SHARES_2);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID_1}, Id{ID_2}});
}

TEST_CASE("multiple sell orders same price no mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID_1 = 3;
    constexpr std::uint64_t ID_2 = 4;
    constexpr std::uint8_t SIDE = 'S';
    constexpr std::uint32_t SHARES_1 = 30;
    constexpr std::uint32_t SHARES_2 = 60;
    constexpr std::uint32_t PRICE = 5000;
    constexpr std::uint64_t TS_1 = 300;
    constexpr std::uint64_t TS_2 = 400;

    auto ob = OrderBook();
    const auto buf1 = build_add_order_no_mpid(ID_1, SIDE, SHARES_1, PRICE, TS_1);
    const auto buf2 = build_add_order_no_mpid(ID_2, SIDE, SHARES_2, PRICE, TS_2);

    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf1));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf2));

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 2);
    REQUIRE(level->volume.value == SHARES_1 + SHARES_2);
    REQUIRE(level_ids(ob, Side::SELL, PRICE) == std::vector<Id>{Id{ID_1}, Id{ID_2}});
}

TEST_CASE("bids sorted high to low no mpid", "[order_book][add_order]") {
    constexpr std::uint8_t SIDE = 'B';
    constexpr std::uint32_t SHARES = 10;
    constexpr std::uint32_t PRICE_LOW = 10000;
    constexpr std::uint32_t PRICE_MID = 15000;
    constexpr std::uint32_t PRICE_HIGH = 20000;
    constexpr std::uint64_t TS_1 = 1;
    constexpr std::uint64_t TS_2 = 2;
    constexpr std::uint64_t TS_3 = 3;

    auto ob = OrderBook();
    const auto buf1 = build_add_order_no_mpid(10, SIDE, SHARES, PRICE_LOW, TS_1);
    const auto buf2 = build_add_order_no_mpid(11, SIDE, SHARES, PRICE_HIGH, TS_2);
    const auto buf3 = build_add_order_no_mpid(12, SIDE, SHARES, PRICE_MID, TS_3);

    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf1));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf2));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf3));

    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(PRICE_HIGH));

    const auto* level_low = ob.get_price_level(Side::BUY, Price{PRICE_LOW});
    REQUIRE(level_low != nullptr);
    REQUIRE(level_low->count == 1);

    const auto* level_mid = ob.get_price_level(Side::BUY, Price{PRICE_MID});
    REQUIRE(level_mid != nullptr);
    REQUIRE(level_mid->count == 1);

    const auto* level_high = ob.get_price_level(Side::BUY, Price{PRICE_HIGH});
    REQUIRE(level_high != nullptr);
    REQUIRE(level_high->count == 1);
}

TEST_CASE("asks sorted low to high no mpid", "[order_book][add_order]") {
    constexpr std::uint8_t SIDE = 'S';
    constexpr std::uint32_t SHARES = 10;
    constexpr std::uint32_t PRICE_LOW = 10000;
    constexpr std::uint32_t PRICE_MID = 15000;
    constexpr std::uint32_t PRICE_HIGH = 20000;
    constexpr std::uint64_t TS_1 = 1;
    constexpr std::uint64_t TS_2 = 2;
    constexpr std::uint64_t TS_3 = 3;

    auto ob = OrderBook();
    const auto buf1 = build_add_order_no_mpid(20, SIDE, SHARES, PRICE_HIGH, TS_1);
    const auto buf2 = build_add_order_no_mpid(21, SIDE, SHARES, PRICE_LOW, TS_2);
    const auto buf3 = build_add_order_no_mpid(22, SIDE, SHARES, PRICE_MID, TS_3);

    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf1));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf2));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf3));

    REQUIRE(best_ask_price(ob) == static_cast<std::uint64_t>(PRICE_LOW));

    const auto* level_low = ob.get_price_level(Side::SELL, Price{PRICE_LOW});
    REQUIRE(level_low != nullptr);
    REQUIRE(level_low->count == 1);

    const auto* level_mid = ob.get_price_level(Side::SELL, Price{PRICE_MID});
    REQUIRE(level_mid != nullptr);
    REQUIRE(level_mid->count == 1);

    const auto* level_high = ob.get_price_level(Side::SELL, Price{PRICE_HIGH});
    REQUIRE(level_high != nullptr);
    REQUIRE(level_high->count == 1);
}

// ---- on_add_order_with_mpid ----

TEST_CASE("add buy order with mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID = 55;
    constexpr std::uint8_t SIDE = 'B';
    constexpr std::uint32_t SHARES = 300;
    constexpr std::uint32_t PRICE = 18000;
    constexpr std::uint64_t TS = 5'000'000;
    constexpr std::uint32_t ATTRIBUTION = 888;

    auto ob = OrderBook();
    const auto buf = build_add_order_with_mpid(ID, SIDE, SHARES, PRICE, TS, ATTRIBUTION);
    const auto* msg = AddOrderWithMPID::new_msg(buf);
    REQUIRE(msg != nullptr);

    const auto slab = ob.add_order_with_mpid(*msg);
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == SHARES);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == SHARES);
    REQUIRE(order.side == Side::BUY);
    REQUIRE(order.price.value == PRICE);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

TEST_CASE("add sell order with mpid", "[order_book][add_order]") {
    constexpr std::uint64_t ID = 77;
    constexpr std::uint8_t SIDE = 'S';
    constexpr std::uint32_t SHARES = 150;
    constexpr std::uint32_t PRICE = 12000;
    constexpr std::uint64_t TS = 3'000'000;
    constexpr std::uint32_t ATTRIBUTION = 444;

    auto ob = OrderBook();
    const auto buf = build_add_order_with_mpid(ID, SIDE, SHARES, PRICE, TS, ATTRIBUTION);
    const auto* msg = AddOrderWithMPID::new_msg(buf);
    REQUIRE(msg != nullptr);

    const auto slab = ob.add_order_with_mpid(*msg);
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == SHARES);
    REQUIRE(level_ids(ob, Side::SELL, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == SHARES);
    REQUIRE(order.side == Side::SELL);
    REQUIRE(order.price.value == PRICE);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("mixed buy sell with mpid", "[order_book][add_order]") {
    constexpr std::uint64_t BUY_ID = 1;
    constexpr std::uint32_t BUY_SHARES = 100;
    constexpr std::uint32_t BUY_PRICE = 10000;
    constexpr std::uint64_t BUY_TS = 111;

    constexpr std::uint64_t SELL_ID = 2;
    constexpr std::uint32_t SELL_SHARES = 50;
    constexpr std::uint32_t SELL_PRICE = 12000;
    constexpr std::uint64_t SELL_TS = 222;
    constexpr std::uint32_t ATTRIBUTION = 1;

    auto ob = OrderBook();
    const auto buf_buy =
        build_add_order_with_mpid(BUY_ID, 'B', BUY_SHARES, BUY_PRICE, BUY_TS, ATTRIBUTION);
    const auto buf_sell =
        build_add_order_with_mpid(SELL_ID, 'S', SELL_SHARES, SELL_PRICE, SELL_TS, ATTRIBUTION);

    ob.add_order_with_mpid(*AddOrderWithMPID::new_msg(buf_buy));
    ob.add_order_with_mpid(*AddOrderWithMPID::new_msg(buf_sell));

    const auto* buy_level = ob.get_price_level(Side::BUY, Price{BUY_PRICE});
    REQUIRE(buy_level != nullptr);
    REQUIRE(buy_level->count == 1);

    const auto* sell_level = ob.get_price_level(Side::SELL, Price{SELL_PRICE});
    REQUIRE(sell_level != nullptr);
    REQUIRE(sell_level->count == 1);

    REQUIRE(level_ids(ob, Side::BUY, BUY_PRICE) == std::vector<Id>{Id{BUY_ID}});
    REQUIRE(level_ids(ob, Side::SELL, SELL_PRICE) == std::vector<Id>{Id{SELL_ID}});
    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(BUY_PRICE));
    REQUIRE(best_ask_price(ob) == static_cast<std::uint64_t>(SELL_PRICE));
}

TEST_CASE("empty order book", "[order_book]") {
    const auto ob = OrderBook();
    REQUIRE(ob.is_empty());
    REQUIRE(ob.best_bid() == 0);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
    REQUIRE(ob.price_offset_is_null());
}

// ---- on_order_execute ----

TEST_CASE("execute full buy order", "[order_book][execute]") {
    constexpr std::uint64_t ID = 10;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t SHARES = 200;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE(ob.handle_execution_slab(slab, SHARES));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("execute partial buy order", "[order_book][execute]") {
    constexpr std::uint64_t ID = 11;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t INITIAL_SHARES = 200;
    constexpr std::uint32_t EXECUTED = 80;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', INITIAL_SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE_FALSE(ob.handle_execution_slab(slab, EXECUTED));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == INITIAL_SHARES - EXECUTED);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == INITIAL_SHARES - EXECUTED);
}

TEST_CASE("execute two partials become full", "[order_book][execute]") {
    constexpr std::uint64_t ID = 12;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t INITIAL_SHARES = 200;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', INITIAL_SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE_FALSE(ob.handle_execution_slab(slab, 50));
    REQUIRE(ob.handle_execution_slab(slab, INITIAL_SHARES - 50));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("execute full sell order", "[order_book][execute]") {
    constexpr std::uint64_t ID = 20;
    constexpr std::uint32_t PRICE = 15000;
    constexpr std::uint32_t SHARES = 300;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'S', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE(ob.handle_execution_slab(slab, SHARES));

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

// ---- on_order_execute_with_price ----

TEST_CASE("execute with price full buy order", "[order_book][execute]") {
    constexpr std::uint64_t ID = 30;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t SHARES = 100;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE(ob.handle_execution_slab(slab, SHARES));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("execute with price partial buy order", "[order_book][execute]") {
    constexpr std::uint64_t ID = 31;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t INITIAL_SHARES = 200;
    constexpr std::uint32_t EXECUTED = 75;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', INITIAL_SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE_FALSE(ob.handle_execution_slab(slab, EXECUTED));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == INITIAL_SHARES - EXECUTED);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == INITIAL_SHARES - EXECUTED);
}

// ---- on_order_delete ----

TEST_CASE("delete existing buy order", "[order_book][delete]") {
    constexpr std::uint64_t ID = 40;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t SHARES = 100;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    ob.remove_slab(slab);

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("delete one of two at same price", "[order_book][delete]") {
    constexpr std::uint64_t ID_1 = 50;
    constexpr std::uint64_t ID_2 = 51;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t SHARES = 100;

    auto ob = OrderBook();
    const auto buf1 = build_add_order_no_mpid(ID_1, 'B', SHARES, PRICE, 1);
    const auto buf2 = build_add_order_no_mpid(ID_2, 'B', SHARES, PRICE, 2);

    const auto slab_1 = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf1));
    const auto slab_2 = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf2));
    REQUIRE(slab_1 != SlabIndex::NULL_INDEX);
    REQUIRE(slab_2 != SlabIndex::NULL_INDEX);

    ob.remove_slab(slab_1);

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID_2}});
    REQUIRE(ob.order_pool_len() == 1);
    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(PRICE));
}

TEST_CASE("delete existing sell order", "[order_book][delete]") {
    constexpr std::uint64_t ID = 60;
    constexpr std::uint32_t PRICE = 15000;
    constexpr std::uint32_t SHARES = 200;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'S', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    ob.remove_slab(slab);

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

// ---- on_order_cancel ----

TEST_CASE("cancel all shares removes order", "[order_book][cancel]") {
    constexpr std::uint64_t ID = 70;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t SHARES = 500;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE(ob.handle_execution_slab(slab, SHARES));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("cancel partial shares keeps order", "[order_book][cancel]") {
    constexpr std::uint64_t ID = 71;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t INITIAL_SHARES = 500;
    constexpr std::uint32_t CANCELLED = 200;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', INITIAL_SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE_FALSE(ob.handle_execution_slab(slab, CANCELLED));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
    REQUIRE(level->volume.value == INITIAL_SHARES - CANCELLED);
    REQUIRE(level_ids(ob, Side::BUY, PRICE) == std::vector<Id>{Id{ID}});

    const auto order = ob.order_pool_get(slab);
    REQUIRE(order.shares.value == INITIAL_SHARES - CANCELLED);
}

TEST_CASE("cancel two partials become full", "[order_book][cancel]") {
    constexpr std::uint64_t ID = 72;
    constexpr std::uint32_t PRICE = 10000;
    constexpr std::uint32_t INITIAL_SHARES = 400;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'B', INITIAL_SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE_FALSE(ob.handle_execution_slab(slab, 150));
    REQUIRE(ob.handle_execution_slab(slab, INITIAL_SHARES - 150));

    const PriceLevel* level = ob.get_price_level(Side::BUY, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_bid() == 0);
}

TEST_CASE("cancel all sell order", "[order_book][cancel]") {
    constexpr std::uint64_t ID = 80;
    constexpr std::uint32_t PRICE = 15000;
    constexpr std::uint32_t SHARES = 300;

    auto ob = OrderBook();
    const auto buf = build_add_order_no_mpid(ID, 'S', SHARES, PRICE, 1);
    const auto slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));
    REQUIRE(slab != SlabIndex::NULL_INDEX);

    REQUIRE(ob.handle_execution_slab(slab, SHARES));

    const PriceLevel* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 0);
    REQUIRE(ob.order_pool_len() == 0);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

// ---- on_order_replace ----

TEST_CASE("replace buy order new price and shares", "[order_book][replace]") {
    constexpr std::uint64_t OLD_ID = 90;
    constexpr std::uint64_t NEW_ID = 91;
    constexpr std::uint32_t OLD_PRICE = 10000;
    constexpr std::uint32_t NEW_PRICE = 12000;
    constexpr std::uint32_t OLD_SHARES = 100;
    constexpr std::uint32_t NEW_SHARES = 250;

    auto ob = OrderBook();
    const auto buf_add = build_add_order_no_mpid(OLD_ID, 'B', OLD_SHARES, OLD_PRICE, 1);
    const auto old_slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf_add));
    REQUIRE(old_slab != SlabIndex::NULL_INDEX);

    const auto buf_replace = build_order_replace(OLD_ID, NEW_ID, NEW_SHARES, NEW_PRICE, 200);
    const auto new_slab = ob.replace_slab(old_slab, *OrderReplace::new_msg(buf_replace));
    REQUIRE(new_slab != SlabIndex::NULL_INDEX);

    const auto* old_level = ob.get_price_level(Side::BUY, Price{OLD_PRICE});
    REQUIRE(old_level != nullptr);
    REQUIRE(old_level->count == 0);
    REQUIRE(ob.order_pool_len() == 1);

    const auto* new_level = ob.get_price_level(Side::BUY, Price{NEW_PRICE});
    REQUIRE(new_level != nullptr);
    REQUIRE(new_level->count == 1);
    REQUIRE(level_ids(ob, Side::BUY, NEW_PRICE) == std::vector<Id>{Id{NEW_ID}});

    const auto new_order = ob.order_pool_get(new_slab);
    REQUIRE(new_order.shares.value == NEW_SHARES);
    REQUIRE(new_order.side == Side::BUY);
    REQUIRE(new_order.price.value == NEW_PRICE);
    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(NEW_PRICE));
}

TEST_CASE("replace sell order same price", "[order_book][replace]") {
    constexpr std::uint64_t OLD_ID = 100;
    constexpr std::uint64_t NEW_ID = 101;
    constexpr std::uint32_t PRICE = 15000;
    constexpr std::uint32_t OLD_SHARES = 100;
    constexpr std::uint32_t NEW_SHARES = 300;

    auto ob = OrderBook();
    const auto buf_add1 = build_add_order_no_mpid(OLD_ID, 'S', OLD_SHARES, PRICE, 1);
    const auto old_slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf_add1));
    REQUIRE(old_slab != SlabIndex::NULL_INDEX);

    const auto buf_add2 = build_add_order_no_mpid(102, 'S', 50, PRICE, 2);
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf_add2));

    const auto buf_replace = build_order_replace(OLD_ID, NEW_ID, NEW_SHARES, PRICE, 200);
    const auto new_slab = ob.replace_slab(old_slab, *OrderReplace::new_msg(buf_replace));
    REQUIRE(new_slab != SlabIndex::NULL_INDEX);

    const auto* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 2);
    REQUIRE(level_ids(ob, Side::SELL, PRICE) == std::vector<Id>{Id{102}, Id{NEW_ID}});
    REQUIRE(ob.order_pool_len() == 2);
}

TEST_CASE("replace preserves side", "[order_book][replace]") {
    constexpr std::uint64_t OLD_ID = 110;
    constexpr std::uint64_t NEW_ID = 111;
    constexpr std::uint32_t PRICE = 10000;

    auto ob = OrderBook();
    const auto buf_add = build_add_order_no_mpid(OLD_ID, 'S', 100, PRICE, 1);
    const auto old_slab = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf_add));
    REQUIRE(old_slab != SlabIndex::NULL_INDEX);

    const auto buf_replace = build_order_replace(OLD_ID, NEW_ID, 200, PRICE, 200);
    const auto new_slab = ob.replace_slab(old_slab, *OrderReplace::new_msg(buf_replace));
    REQUIRE(new_slab != SlabIndex::NULL_INDEX);

    const auto* level = ob.get_price_level(Side::SELL, Price{PRICE});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);

    const auto new_order = ob.order_pool_get(new_slab);
    REQUIRE(new_order.side == Side::SELL);
}

// ---- on_system_event ----

TEST_CASE("system event clears non empty book", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(2, 'S', 50, 12000, 2)));
    REQUIRE_FALSE(ob.is_empty());

    const auto buf = build_system_event('O', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE(ob.is_empty());
    REQUIRE(ob.best_bid() == 0);
    REQUIRE(ob.best_ask() == std::numeric_limits<std::uint32_t>::max());
}

TEST_CASE("system event start of system hours clears", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));

    const auto buf = build_system_event('S', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE(ob.is_empty());
}

TEST_CASE("system event end of system hours clears", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));

    const auto buf = build_system_event('E', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE(ob.is_empty());
}

TEST_CASE("system event end of messages clears", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));

    const auto buf = build_system_event('C', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE(ob.is_empty());
}

TEST_CASE("system event on empty book no panic", "[order_book][system_event]") {
    auto ob = OrderBook();
    REQUIRE(ob.is_empty());

    const auto buf = build_system_event('O', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE(ob.is_empty());
}

TEST_CASE("system event market hours does not clear", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));

    const auto buf = build_system_event('Q', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE_FALSE(ob.is_empty());
    const auto* level = ob.get_price_level(Side::BUY, Price{10000});
    REQUIRE(level != nullptr);
    REQUIRE(level->count == 1);
}

TEST_CASE("system event end of market hours does not clear", "[order_book][system_event]") {
    auto ob = OrderBook();
    ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(build_add_order_no_mpid(1, 'B', 100, 10000, 1)));

    const auto buf = build_system_event('M', 100);
    ob.on_system_event(*SystemEventMessage::new_msg(buf));

    REQUIRE_FALSE(ob.is_empty());
}

// ---- price offset sliding ----

TEST_CASE("lower price shifts offset down", "[order_book][offset]") {
    constexpr std::uint64_t HIGH_ID = 1;
    constexpr std::uint64_t LOW_ID = 2;
    constexpr std::uint32_t HIGH_PRICE = 20'000;
    constexpr std::uint32_t LOW_PRICE = 10'000;

    auto ob = OrderBook();
    const auto high_slab = ob.add_order_no_mpid(
        *AddOrderNoMPID::new_msg(build_add_order_no_mpid(HIGH_ID, 'B', 100, HIGH_PRICE, 1)));
    REQUIRE(high_slab != SlabIndex::NULL_INDEX);
    REQUIRE(ob.price_offset() == static_cast<std::uint64_t>(HIGH_PRICE) / TICK_SIZE);
    REQUIRE(ob.best_bid() == 0);

    ob.add_order_no_mpid(
        *AddOrderNoMPID::new_msg(build_add_order_no_mpid(LOW_ID, 'B', 100, LOW_PRICE, 2)));
    REQUIRE(ob.price_offset() == static_cast<std::uint64_t>(LOW_PRICE) / TICK_SIZE);
    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(HIGH_PRICE));

    const auto* high_level = ob.get_price_level(Side::BUY, Price{HIGH_PRICE});
    REQUIRE(high_level != nullptr);
    REQUIRE(high_level->count == 1);

    const auto* low_level = ob.get_price_level(Side::BUY, Price{LOW_PRICE});
    REQUIRE(low_level != nullptr);
    REQUIRE(low_level->count == 1);

    REQUIRE(level_ids(ob, Side::BUY, HIGH_PRICE) == std::vector<Id>{Id{HIGH_ID}});
    REQUIRE(level_ids(ob, Side::BUY, LOW_PRICE) == std::vector<Id>{Id{LOW_ID}});

    ob.remove_slab(high_slab);
    REQUIRE(best_bid_price(ob) == static_cast<std::uint64_t>(LOW_PRICE));
    REQUIRE(ob.order_pool_len() == 1);
}

TEST_CASE("out of band order is rejected", "[order_book][bounds]") {
    constexpr std::uint64_t IN_BAND_ID = 1;
    constexpr std::uint64_t OUT_OF_BAND_ID = 2;
    constexpr std::uint32_t BASE_PRICE = 10'000;
    constexpr std::uint32_t OUT_OF_BAND_PRICE = 10'010'000;

    auto ob = OrderBook();
    ob.add_order_no_mpid(
        *AddOrderNoMPID::new_msg(build_add_order_no_mpid(IN_BAND_ID, 'B', 100, BASE_PRICE, 1)));

    const auto buf = build_add_order_no_mpid(OUT_OF_BAND_ID, 'B', 100, OUT_OF_BAND_PRICE, 2);
    const auto rejected = ob.add_order_no_mpid(*AddOrderNoMPID::new_msg(buf));

    REQUIRE(rejected == SlabIndex::NULL_INDEX);
    REQUIRE(ob.order_pool_len() == 1);
    REQUIRE(ob.get_price_level(Side::BUY, Price{OUT_OF_BAND_PRICE}) == nullptr);
    REQUIRE(level_ids(ob, Side::BUY, BASE_PRICE) == std::vector<Id>{Id{IN_BAND_ID}});
    REQUIRE(ob.best_bid() == 0);
}
