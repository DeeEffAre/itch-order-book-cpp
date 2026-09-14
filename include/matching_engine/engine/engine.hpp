#pragma once

#include "matching_engine/itch_parser/messages.hpp"
#include "matching_engine/order_book/book.hpp"

struct OrderLookupInfo {
    SlabIndex slab_idx;
    std::uint16_t locate;

    static const OrderLookupInfo NULL_INFO;

    constexpr OrderLookupInfo(SlabIndex slab_idx, std::uint16_t locate)
        : slab_idx(slab_idx), locate(locate) {};

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return slab_idx == SlabIndex::NULL_INDEX && locate == 0;
    }
};

inline constexpr OrderLookupInfo OrderLookupInfo::NULL_INFO{
    OrderLookupInfo{SlabIndex::NULL_INDEX, 0}};

class GlobalMap {
    std::vector<OrderLookupInfo> map_;

public:
    // returns `OrderLookupInfo::NULL_INFO` at wrong path
    [[nodiscard]] OrderLookupInfo get(std::uint64_t order_ref) const noexcept;
    void set(std::uint64_t order_ref, std::uint16_t locate, SlabIndex slab_idx) noexcept;
    void remove(std::uint64_t order_ref) noexcept;
    void clear() noexcept { map_.clear(); }
};

class Engine {
    std::vector<std::optional<OrderBook>> books_;
    GlobalMap global_map_;

public:
    OrderBook& inline_get_book(std::size_t locate) noexcept;
    void on_system_event(const SystemEventMessage* msg);
    void on_add_order_no_mpid(const AddOrderNoMPID* msg);
    void on_add_order_with_mpid(const AddOrderWithMPID* msg);
    void on_order_cancel(const OrderCancel* msg);
    void on_order_delete(const OrderDelete* msg);
    void on_order_replace(const OrderReplace* msg);
    void on_order_execute(const OrderExecute* msg);
    void on_order_execute_with_price(const OrderExecuteWithPrice* msg);
};
