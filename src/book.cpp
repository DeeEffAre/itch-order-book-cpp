#include "matching_engine/itch_parser/messages.hpp"
#include "matching_engine/order_book/constants.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <matching_engine/order_book/book.hpp>

SlabIndex Slab::insert(Order order) noexcept {
    if (free_list.size() != 0) {
        const auto idx = free_list.back();
        free_list.pop_back();
        orders[idx.value] = order;
        return idx;
    }

    const auto idx = orders.size();
    orders.emplace_back(order);
    return SlabIndex{static_cast<std::uint32_t>(idx)};
}

void Slab::remove(SlabIndex idx) noexcept {
    free_list.push_back(idx);
}

const PriceLevel* OrderBook::get_price_level(Side side, Price price) const noexcept {
    if (price_offset_.is_null()) [[unlikely]] {
        return nullptr;
    }

    const auto offset = price_offset_.value;
    const auto tick_price = price.value / TICK_SIZE;

    if (tick_price < offset) {
        return nullptr;
    }

    const auto idx = (tick_price - offset);
    if (idx >= MAX_PRICE) {
        return nullptr;
    }

    return (side == Side::BUY) ? &bids_[idx] : &asks_[idx]; // NOLINT
}

SlabIndex OrderBook::add_order_no_mpid(const AddOrderNoMPID& msg) noexcept {
    auto order = Order(msg);
    return this->add_order(order);
}

SlabIndex OrderBook::add_order_with_mpid(const AddOrderWithMPID& msg) noexcept {
    auto order = Order(msg);
    return this->add_order(order);
}

void OrderBook::remove_slab(SlabIndex slab_idx) noexcept {
    this->remove_order_by_slab(slab_idx);
}

bool OrderBook::handle_execution_slab(SlabIndex slab_idx, std::uint32_t executed_shares) noexcept {
    return this->handle_execution(slab_idx, executed_shares);
}

SlabIndex OrderBook::replace_slab(SlabIndex old_slab_idx, const OrderReplace& msg) noexcept {
    const auto old_order_side = order_pool_[old_slab_idx].side;
    this->remove_order_by_slab(old_slab_idx);

    auto new_order = Order(msg);
    new_order.side = old_order_side;
    return this->add_order(new_order);
}

void OrderBook::on_system_event(const SystemEventMessage& msg) noexcept {
    switch (static_cast<char>(msg.event_code)) {
    case 'O':
    case 'S':
    case 'E':
    case 'C':
        if (!this->is_empty()) {
            this->clear_all();
        }
        break;
    default:
        return;
    }
}

void OrderBook::shift_prices_down(std::uint64_t shift) noexcept {
#ifndef NDEBUG
    if (price_offset_.is_null()) {
        assert(false && "CRITICAL: Shift attempted but price_offset_ is null!");
    }
#else
    if (price_offset_.is_null()) {
        std::unreachable();
    }
#endif

    const auto new_offset = price_offset_.value - shift;

    // TODO: more proper handling maybe?
    assert(shift < MAX_PRICE);

    this->collect_active_level_indices();

    for (auto& level : bids_) {
        level = PriceLevel{};
    }
    for (auto& level : asks_) {
        level = PriceLevel{};
    }

    bid_bitmap_->clear_all();
    ask_bitmap_->clear_all();
    best_bid_ = 0;
    best_ask_ = std::numeric_limits<std::uint32_t>::max();

    for (auto slab_idx : active_orders_scratchpad_) {
        const Order& order = order_pool_[slab_idx];
        const auto tick_price = order.price.value / TICK_SIZE;

        if (tick_price < new_offset) [[unlikely]] {
            // out of band rejection
            continue;
        }

        const auto new_price_idx = (tick_price - new_offset);
        if (new_price_idx >= MAX_PRICE) [[unlikely]] {
            // out of band rejection
            continue;
        }

        PriceLevel* levels[2] = {&bids_[new_price_idx], &asks_[new_price_idx]};
        PriceLevel* level = levels[static_cast<std::size_t>(order.side)];

        std::uint32_t* const targets[] = {&best_bid_, &best_ask_};
        std::uint32_t* target = targets[static_cast<std::size_t>(order.side)];

        using AlgorithmPtr = const std::uint32_t& (*)(const std::uint32_t&, const std::uint32_t&);
        const AlgorithmPtr algorithms[] = {static_cast<AlgorithmPtr>(std::max<std::uint32_t>),
                                           static_cast<AlgorithmPtr>(std::min<std::uint32_t>)};
        auto algo = algorithms[static_cast<std::size_t>(order.side)];

        *target = algo(*target, static_cast<std::uint32_t>(new_price_idx));

        order_pool_[slab_idx].prev = SlabIndex::NULL_INDEX;
        order_pool_[slab_idx].next = SlabIndex::NULL_INDEX;

        if (level->tail == SlabIndex::NULL_INDEX) {
            level->head = slab_idx;
            level->tail = slab_idx;
        } else {
            auto old_tail = level->tail;
            order_pool_[old_tail].next = slab_idx;
            order_pool_[slab_idx].prev = old_tail;
            level->tail = slab_idx;
        }

        if (level->count == 0) {
            PriceBitset* bitmaps[2] = {&*bid_bitmap_, &*ask_bitmap_};
            PriceBitset* bitmap = bitmaps[static_cast<std::size_t>(order.side)];
            bitmap->set(new_price_idx);
        }

        level->volume.add(order.shares);
        level->count += 1;
    }

    price_offset_.value = new_offset;
}

void OrderBook::collect_active_level_indices() noexcept {
    active_orders_scratchpad_.clear();
    auto collect_levels = [this](const auto& levels, const PriceBitset& bitmap) {
        constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();
        for (std::size_t price_idx = bitmap.find_first_set_from(0); price_idx != NONE;
             price_idx = bitmap.find_first_set_from(price_idx + 1)) {
            for (auto idx = levels[price_idx].head; idx != SlabIndex::NULL_INDEX;
                 idx = order_pool_[idx].next) {
                active_orders_scratchpad_.push_back(idx);
            }
        }
    };
    collect_levels(bids_, *bid_bitmap_);
    collect_levels(asks_, *ask_bitmap_);
}

SlabIndex OrderBook::add_order(Order order) noexcept {
    const auto price = order.price.value;

    // tick scaling: 4dp wire value -> 1-cent ticks
    const auto tick_price = price / TICK_SIZE;

    if (price_offset_.is_null()) {
        price_offset_.value = tick_price;
    }

    auto offset = price_offset_.value;
    if (tick_price < offset) {
        const auto shift = offset - tick_price;
        if (shift >= MAX_PRICE) [[unlikely]] {
            return SlabIndex::NULL_INDEX;
        }
        this->shift_prices_down(shift);
        price_offset_.value = tick_price;
    }

    offset = price_offset_.value;

    const auto price_idx = tick_price - offset;
    if (price_idx >= MAX_PRICE) [[unlikely]] {
        return SlabIndex::NULL_INDEX;
    }

    const auto slab_idx = order_pool_.insert(order);

    PriceLevel* levels[2] = {&bids_[price_idx], &asks_[price_idx]};
    PriceLevel* level = levels[static_cast<std::size_t>(order.side)];

    std::uint32_t* const targets[] = {&best_bid_, &best_ask_};
    std::uint32_t* target = targets[static_cast<std::size_t>(order.side)];

    using AlgorithmPtr = const std::uint32_t& (*)(const std::uint32_t&, const std::uint32_t&);
    const AlgorithmPtr algorithms[] = {static_cast<AlgorithmPtr>(std::max<std::uint32_t>),
                                       static_cast<AlgorithmPtr>(std::min<std::uint32_t>)};
    auto algo = algorithms[static_cast<std::size_t>(order.side)];

    *target = algo(*target, static_cast<std::uint32_t>(price_idx));

    if (level->tail == SlabIndex::NULL_INDEX) {
        level->head = slab_idx;
        level->tail = slab_idx;
    } else {
        const auto old_tail = level->tail;
        order_pool_[old_tail].next = slab_idx;
        order_pool_[slab_idx].prev = old_tail;
        level->tail = slab_idx;
    }

    if (level->count == 0) {
        PriceBitset* bitmaps[2] = {&*bid_bitmap_, &*ask_bitmap_};
        PriceBitset* bitmap = bitmaps[static_cast<std::size_t>(order.side)];
        bitmap->set(price_idx);
    }

    level->volume.add(order.shares);
    level->count += 1;

    return slab_idx;
}

void OrderBook::remove_order_by_slab(SlabIndex slab_idx) noexcept {
    auto order_copy = order_pool_[slab_idx];
    const auto tick_price = order_copy.price.value / TICK_SIZE;

#ifndef NDEBUG
    if (price_offset_.is_null()) {
        assert(false && "CRITICAL: Removing order but price_offset_ is null!");
    }
#else
    if (price_offset_.is_null()) {
        std::unreachable();
    }
#endif

    const auto offset = price_offset_.value;
    const auto price_idx = tick_price - offset;

    if (price_idx >= MAX_PRICE) [[unlikely]] {
        return;
    }

    PriceLevel* levels[2] = {&bids_[price_idx], &asks_[price_idx]};
    PriceLevel* level = levels[static_cast<std::size_t>(order_copy.side)];

    level->volume.sub(order_copy.shares);

    remove_from_pool_and_bitset(slab_idx, order_copy, *level, price_idx);
}

bool OrderBook::handle_execution(SlabIndex slab_idx, std::uint32_t executed_shares) noexcept {
    auto order_copy = order_pool_[slab_idx];
    order_copy.shares.value -= executed_shares;
    const auto fully_done = order_copy.shares.value == 0;

    const auto tick_price = order_copy.price.value / TICK_SIZE;

#ifndef NDEBUG
    if (price_offset_.is_null()) {
        assert(false && "CRITICAL: Handling execution but price_offset_ is null!");
    }
#else
    if (price_offset_.is_null()) {
        std::unreachable();
    }
#endif

    const auto offset = price_offset_.value;
    const auto price_idx = tick_price - offset;

    if (price_idx >= MAX_PRICE) [[unlikely]] {
        return false;
    }

    PriceLevel* levels[2] = {&bids_[price_idx], &asks_[price_idx]};
    PriceLevel* level = levels[static_cast<std::size_t>(order_copy.side)];

    level->volume.sub(Quantity{executed_shares});

    if (fully_done) {
        remove_from_pool_and_bitset(slab_idx, order_copy, *level, price_idx);
        return true;
    }

    order_pool_[slab_idx] = order_copy;
    return false;
}

void OrderBook::remove_from_pool_and_bitset(SlabIndex slab_idx, const Order& order_copy,
                                            PriceLevel& level, std::uint64_t price_idx) noexcept {
    order_pool_.remove(slab_idx);

    const auto prev = order_copy.prev;
    const auto next = order_copy.next;

    if (prev.is_null()) {
        level.head = next;
    } else {
        order_pool_[prev].next = next;
    }

    if (next.is_null()) {
        level.tail = prev;
    } else {
        order_pool_[next].prev = prev;
    }
    level.count -= 1;

    if (level.count == 0) [[unlikely]] {
        if (order_copy.side == Side::BUY) {
            bid_bitmap_->clear(price_idx);
            if (price_idx == best_bid_) {
                const auto best = bid_bitmap_->find_last_set_from(price_idx);
                best_bid_ = (best == std::numeric_limits<std::size_t>::max())
                                ? 0
                                : static_cast<std::uint32_t>(best);
            }
        } else {
            ask_bitmap_->clear(price_idx);
            if (price_idx == best_ask_) {
                const auto best = ask_bitmap_->find_first_set_from(price_idx);
                best_ask_ = static_cast<std::uint32_t>(best);
            }
        }
    }
}

bool Slab::is_empty() const noexcept {
    return orders.size() == free_list.size();
}

void Slab::clear() noexcept {
    orders.clear();
    free_list.clear();
}

bool OrderBook::is_empty() const noexcept {
    return order_pool_.is_empty();
}

void OrderBook::clear_all() noexcept {
    for (auto& level : bids_) {
        level = PriceLevel{};
    }

    for (auto& level : asks_) {
        level = PriceLevel{};
    }

    price_offset_ = PriceOffset::NULL_INDEX;
    order_pool_.clear();
    best_bid_ = 0;
    best_ask_ = std::numeric_limits<std::uint32_t>::max();
    bid_bitmap_->clear_all();
    ask_bitmap_->clear_all();
}
