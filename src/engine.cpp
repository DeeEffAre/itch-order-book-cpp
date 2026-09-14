#include "matching_engine/engine/engine.hpp"

OrderLookupInfo GlobalMap::get(std::uint64_t order_ref) const noexcept {
    const OrderLookupInfo info = map_[order_ref];
    if (info.slab_idx.is_null()) {
        return OrderLookupInfo::NULL_INFO;
    }
    return info;
}

void GlobalMap::set(std::uint64_t order_ref, std::uint16_t locate, SlabIndex slab_idx) noexcept {
    if (order_ref >= map_.size()) {
        map_.resize(order_ref + 1, OrderLookupInfo::NULL_INFO);
    }
    map_[order_ref] = OrderLookupInfo{slab_idx, locate};
}

void GlobalMap::remove(std::uint64_t order_ref) noexcept {
    OrderLookupInfo* info = &map_[order_ref];
    *info = OrderLookupInfo::NULL_INFO;
}

OrderBook& Engine::inline_get_book(std::size_t locate) noexcept {
    if (locate >= books_.size()) {
        books_.resize(locate + 1);
    }

    auto& book_opt = books_[locate];
    if (!book_opt.has_value()) {
        // calls default constructor
        book_opt.emplace();
    }
    return *book_opt;
}

void Engine::on_system_event(const SystemEventMessage* msg) {
    switch (static_cast<char>(msg->event_code)) {
    case 'O':
    case 'S':
    case 'E':
    case 'C':
        global_map_.clear();
        break;
    default:
        return;
    }

    for (auto& book : books_) {
        if (book.has_value()) {
            book->on_system_event(*msg);
        }
    }
}

void Engine::on_add_order_no_mpid(const AddOrderNoMPID* msg) {
    const auto locate = msg->stock_locate.get();
    const auto slab = this->inline_get_book(locate).add_order_no_mpid(*msg);
    global_map_.set(msg->order_ref_number.get(), locate, slab);
}

void Engine::on_add_order_with_mpid(const AddOrderWithMPID* msg) {
    const auto locate = msg->stock_locate.get();
    const auto slab = this->inline_get_book(locate).add_order_with_mpid(*msg);
    global_map_.set(msg->order_ref_number.get(), locate, slab);
}

void Engine::on_order_cancel(const OrderCancel* msg) {
    const OrderLookupInfo info = global_map_.get(msg->order_ref_number.get());
    if (info.is_null()) {
        return;
    }
    auto const [slab_idx, locate] = info;

    const auto fully_done =
        this->inline_get_book(locate).handle_execution_slab(slab_idx, msg->cancelled_shares.get());

    if (fully_done) {
        global_map_.remove(msg->order_ref_number.get());
    }
}

void Engine::on_order_delete(const OrderDelete* msg) {
    const OrderLookupInfo info = global_map_.get(msg->order_ref_number.get());
    if (info.is_null()) {
        return;
    }
    auto const [slab_idx, locate] = info;

    this->inline_get_book(locate).remove_slab(slab_idx);
    global_map_.remove(msg->order_ref_number.get());
}

void Engine::on_order_replace(const OrderReplace* msg) {
    const OrderLookupInfo info = global_map_.get(msg->org_order_ref_number.get());
    if (info.is_null()) {
        return;
    }
    auto const [slab_idx, locate] = info;

    const auto new_slab_idx = this->inline_get_book(locate).replace_slab(slab_idx, *msg);
    if (new_slab_idx.is_null()) {
        return;
    }

    global_map_.set(msg->new_order_ref_number.get(), locate, new_slab_idx);
    global_map_.remove(msg->org_order_ref_number.get());
}

void Engine::on_order_execute(const OrderExecute* msg) {
    const OrderLookupInfo info = global_map_.get(msg->order_ref_number.get());
    if (info.is_null()) {
        return;
    }
    auto const [slab_idx, locate] = info;

    const auto fully_done =
        this->inline_get_book(locate).handle_execution_slab(slab_idx, msg->executed_shares.get());

    if (fully_done) {
        global_map_.remove(msg->order_ref_number.get());
    }
}

void Engine::on_order_execute_with_price(const OrderExecuteWithPrice* msg) {
    const OrderLookupInfo info = global_map_.get(msg->order_ref_number.get());
    if (info.is_null()) {
        return;
    }

    auto const [slab_idx, locate] = info;

    const auto fully_done =
        this->inline_get_book(locate).handle_execution_slab(slab_idx, msg->executed_shares.get());

    if (fully_done) {
        global_map_.remove(msg->order_ref_number.get());
    }
}
