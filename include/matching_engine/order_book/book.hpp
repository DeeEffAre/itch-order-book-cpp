#pragma once

#include "matching_engine/itch_parser/messages.hpp"
#include "matching_engine/order_book/bitset.hpp"
#include "matching_engine/order_book/constants.hpp"
#include "matching_engine/order_book/types.hpp"

#include <cassert>
#include <cstdint>

struct alignas(32) Order {
public:
    Order(const AddOrderNoMPID& value)
        : id(Id(value.order_ref_number.get())), price(Price(value.price.get())),
          shares(value.shares.get()), side(Side::from_wire(value.indicator)) {}
    Order(const AddOrderWithMPID& value)
        : id(Id(value.order_ref_number.get())), price(Price(value.price.get())),
          shares(value.shares.get()), side(Side::from_wire(value.indicator)) {}
    Order(const OrderReplace& value)
        : id(Id(value.new_order_ref_number.get())), price(Price(value.price.get())),
          shares(value.shares.get()), side(Side::from_wire('S')) // dummy value
    {}

    Id id;
    Price price;
    Quantity shares;
    Side side;
    SlabIndex prev{SlabIndex::NULL_INDEX};
    SlabIndex next{SlabIndex::NULL_INDEX};
};

struct PriceLevel {
    SlabIndex head{SlabIndex::NULL_INDEX};
    SlabIndex tail{SlabIndex::NULL_INDEX};
    Volume volume;
    std::uint32_t count{};

    constexpr PriceLevel() = default;
};

struct Slab {
    std::vector<Order> orders;
    std::vector<SlabIndex> free_list;

    [[nodiscard]] constexpr const Order& operator[](SlabIndex index) const noexcept {
        assert(!index.is_null());
        return orders[static_cast<std::size_t>(index.value)];
    }

    [[nodiscard]] constexpr Order& operator[](SlabIndex index) noexcept {
        assert(!index.is_null());
        return orders[static_cast<std::size_t>(index.value)];
    }

    SlabIndex insert(Order order) noexcept;
    void remove(SlabIndex idx) noexcept;
    [[nodiscard]] bool is_empty() const noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t len() const noexcept { return orders.size() - free_list.size(); }
};

class OrderBook {
public:
    OrderBook() {
        // `reserve` only reserves the memory but keeps the size of the vector 0 because of that
        // accessing [] accessing may cause ub
        bids_.resize(MAX_PRICE);
        asks_.resize(MAX_PRICE);
        std::ranges::fill(bids_, PriceLevel{});
        std::ranges::fill(asks_, PriceLevel{});
    }

    OrderBook(const OrderBook& other)
        : price_offset_(other.price_offset_), bids_(other.bids_), asks_(other.asks_),
          bid_bitmap_(std::make_unique<PriceBitset>(*other.bid_bitmap_)),
          ask_bitmap_(std::make_unique<PriceBitset>(*other.ask_bitmap_)),
          order_pool_(other.order_pool_), best_bid_(other.best_bid_), best_ask_(other.best_ask_) {}

    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;
    [[nodiscard]] const PriceLevel* get_price_level(Side side, Price price) const noexcept;

    // returns size_t::MAX at wrong path
    SlabIndex add_order_no_mpid(const AddOrderNoMPID& msg) noexcept;

    // returns size_t::MAX at wrong path
    SlabIndex add_order_with_mpid(const AddOrderWithMPID& msg) noexcept;

    void remove_slab(SlabIndex slab_idx) noexcept;

    bool handle_execution_slab(SlabIndex slab_idx, std::uint32_t executed_shares) noexcept;

    // returns size_t::MAX at wrong path
    SlabIndex replace_slab(SlabIndex old_slab_idx, const OrderReplace& msg) noexcept;

    void on_system_event(const SystemEventMessage& msg) noexcept;

    [[nodiscard]] bool is_empty() const noexcept;

    [[nodiscard]] std::uint64_t price_offset() const noexcept { return price_offset_.value; }
    [[nodiscard]] std::uint32_t best_bid() const noexcept { return best_bid_; }
    [[nodiscard]] std::uint32_t best_ask() const noexcept { return best_ask_; }
    [[nodiscard]] const Order& order_pool_get(SlabIndex slab_idx) const noexcept {
        return order_pool_[slab_idx];
    }
    [[nodiscard]] std::size_t order_pool_len() const noexcept { return order_pool_.len(); }
    [[nodiscard]] bool price_offset_is_null() const noexcept { return price_offset_.is_null(); }

private:
    PriceOffset price_offset_{PriceOffset::NULL_INDEX};
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;

    std::unique_ptr<PriceBitset> bid_bitmap_{std::make_unique<PriceBitset>()};
    std::unique_ptr<PriceBitset> ask_bitmap_{std::make_unique<PriceBitset>()};

    Slab order_pool_{};

    std::uint32_t best_bid_{};
    std::uint32_t best_ask_{std::numeric_limits<std::uint32_t>::max()};

    std::vector<SlabIndex> active_orders_scratchpad_;

    void shift_prices_down(std::uint64_t shift) noexcept;
    void collect_active_level_indices() noexcept;

    // check if the return type if is null with `.is_null` method
    SlabIndex add_order(Order order) noexcept;

    void remove_order_by_slab(SlabIndex slab_idx) noexcept;
    bool handle_execution(SlabIndex slab_idx, std::uint32_t executed_shares) noexcept;
    void remove_from_pool_and_bitset(SlabIndex slab_idx, const Order& order_copy, PriceLevel& level,
                                     std::uint64_t price_idx) noexcept;
    void clear_all() noexcept;
};
