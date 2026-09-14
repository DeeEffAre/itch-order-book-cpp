#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bench::common::synthetic {

constexpr std::uint16_t STOCK_LOCATE = 100;
constexpr std::array<std::uint8_t, 8> STOCK = {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '};
constexpr std::uint32_t MID_PRICE = 500'000; // $50.00
constexpr std::uint32_t HALF_BAND = 30'000;  // $3.00

struct Weights {
    std::uint32_t add_no_mpid{75};
    std::uint32_t add_with_mpid{5};
    std::uint32_t cancel{10};
    std::uint32_t delete_order{2};
    std::uint32_t replace{3};
    std::uint32_t execute{4};
    std::uint32_t execute_with_price{1};
};

struct SyntheticConfig {
    std::uint64_t seed{42};
    std::size_t message_count{1'000'000};
    std::size_t prefill_orders{100'000};
    Weights weights{};
    std::uint16_t stock_locate{STOCK_LOCATE};
    std::array<std::uint8_t, 8> stock{STOCK};
    std::uint32_t mid_price{MID_PRICE};
    std::uint32_t half_band{HALF_BAND};
};

struct Feed {
    std::vector<std::vector<std::uint8_t>> prefill;
    std::vector<std::vector<std::uint8_t>> messages;
    std::size_t final_active_orders{0};
};

struct ActiveOrder {
    std::uint8_t side;
    std::uint32_t price;
    std::uint32_t shares;
};

class Generator {
public:
    explicit Generator(SyntheticConfig cfg) : cfg_(cfg), rng_(cfg_.seed) {}

    void insert(std::uint64_t id, ActiveOrder order) {
        id_to_index_[id] = orders_.size();
        orders_.push_back(order);
        active_ids_.push_back(id);
    }

    void remove(std::uint64_t id) {
        auto it = id_to_index_.find(id);
        if (it == id_to_index_.end()) {
            return;
        }

        std::size_t idx = it->second;
        id_to_index_.erase(it);

        std::size_t last = orders_.size() - 1;
        if (idx != last) {
            orders_[idx] = orders_[last];
            active_ids_[idx] = active_ids_[last];
            id_to_index_[active_ids_[idx]] = idx;
        }
        orders_.pop_back();
        active_ids_.pop_back();
    }

    std::uint64_t next_ts() {
        std::uniform_int_distribution<std::uint64_t> dist(1, 10'000);
        next_timestamp_ += dist(rng_);
        return next_timestamp_;
    }

    std::uint32_t add_price(std::uint8_t side) {
        std::uniform_int_distribution<std::uint32_t> dist(0, cfg_.half_band);
        std::uint32_t offset = dist(rng_);
        if (side == 'B') {
            return (cfg_.mid_price > offset) ? cfg_.mid_price - offset : 0;
        }
        return cfg_.mid_price + offset;
    }

    std::vector<std::uint8_t> add(bool with_mpid) {
        std::uint64_t id = next_ref_++;
        std::uniform_int_distribution<int> coin(0, 1);
        std::uint8_t side = (coin(rng_) != 0) ? 'B' : 'S';

        std::uniform_int_distribution<std::uint32_t> share_dist(1, 1000);
        ActiveOrder order{.side = side, .price = add_price(side), .shares = share_dist(rng_)};
        insert(id, order);

        std::size_t len = with_mpid ? 40 : 36;
        std::vector<std::uint8_t> buf(len, 0);
        buf[0] = with_mpid ? 'F' : 'A';
        w16(buf, 1, cfg_.stock_locate);
        w16(buf, 3, tracking_++);
        w48(buf, 5, next_ts());
        w64(buf, 11, id);
        buf[19] = side;
        w32(buf, 20, order.shares);

        std::uint64_t stock_u64 = 0;
        std::memcpy(&stock_u64, cfg_.stock.data(), 8);
        w64(buf, 24, __builtin_bswap64(stock_u64));

        w32(buf, 32, order.price);
        if (with_mpid) {
            std::uniform_int_distribution<std::uint32_t> mpid_dist;
            w32(buf, 36, mpid_dist(rng_));
        }
        return buf;
    }

    std::uint64_t random_order_id() {
        std::uniform_int_distribution<std::size_t> dist(0, active_ids_.size() - 1);
        return active_ids_[dist(rng_)];
    }

    std::vector<std::uint8_t> cancel() {
        std::uint64_t id = random_order_id();
        std::size_t idx = id_to_index_[id];
        auto& order = orders_[idx];

        std::uniform_int_distribution<std::uint32_t> dist(1, order.shares);
        std::uint32_t cancelled_shares = dist(rng_);
        order.shares -= cancelled_shares;

        if (order.shares == 0) {
            remove(id);
        }

        std::vector<std::uint8_t> buf(23, 0);
        buf[0] = 'X';
        w16(buf, 1, cfg_.stock_locate);
        w16(buf, 3, tracking_++);
        w48(buf, 5, next_ts());
        w64(buf, 11, id);
        w32(buf, 19, cancelled_shares);
        return buf;
    }

    std::vector<std::uint8_t> delete_order() {
        std::uint64_t id = random_order_id();
        remove(id);

        std::vector<std::uint8_t> buf(19, 0);
        buf[0] = 'D';
        w16(buf, 1, cfg_.stock_locate);
        w16(buf, 3, tracking_++);
        w48(buf, 5, next_ts());
        w64(buf, 11, id);
        return buf;
    }

    std::vector<std::uint8_t> replace() {
        std::uint64_t id = random_order_id();
        std::uint8_t side = orders_[id_to_index_[id]].side;
        remove(id);

        std::uint64_t new_id = next_ref_++;
        std::uint32_t price = add_price(side);
        std::uniform_int_distribution<std::uint32_t> dist(1, 1000);
        std::uint32_t shares = dist(rng_);
        insert(new_id, ActiveOrder{.side = side, .price = price, .shares = shares});

        std::vector<std::uint8_t> buf(35, 0);
        buf[0] = 'U';
        w16(buf, 1, cfg_.stock_locate);
        w16(buf, 3, tracking_++);
        w48(buf, 5, next_ts());
        w64(buf, 11, id);
        w64(buf, 19, new_id);
        w32(buf, 27, shares);
        w32(buf, 31, price);
        return buf;
    }

    std::vector<std::uint8_t> execute(bool with_price) {
        std::uint64_t id = random_order_id();
        std::size_t idx = id_to_index_[id];
        auto& order = orders_[idx];
        std::uint32_t order_price = order.price;

        std::uniform_int_distribution<std::uint32_t> dist(1, order.shares);
        std::uint32_t executed_shares = dist(rng_);
        order.shares -= executed_shares;

        if (order.shares == 0) {
            remove(id);
        }

        std::uint64_t match_number = next_match_++;

        if (with_price) {
            std::vector<std::uint8_t> buf(36, 0);
            buf[0] = 'C';
            w16(buf, 1, cfg_.stock_locate);
            w16(buf, 3, tracking_++);
            w48(buf, 5, next_ts());
            w64(buf, 11, id);
            w32(buf, 19, executed_shares);
            w64(buf, 23, match_number);
            buf[31] = 'Y';
            w32(buf, 32, order_price);
            return buf;
        }
        std::vector<std::uint8_t> buf(31, 0);
        buf[0] = 'E';
        w16(buf, 1, cfg_.stock_locate);
        w16(buf, 3, tracking_++);
        w48(buf, 5, next_ts());
        w64(buf, 11, id);
        w32(buf, 19, executed_shares);
        w64(buf, 23, match_number);
        return buf;
    }

    std::uint8_t pick_type() {
        const auto& w = cfg_.weights;
        std::uint32_t total = w.add_no_mpid + w.add_with_mpid + w.cancel + w.delete_order +
                              w.replace + w.execute + w.execute_with_price;
        std::uniform_int_distribution<std::uint32_t> dist(0, total - 1);
        std::uint32_t r = dist(rng_);

        const std::pair<std::uint32_t, std::uint8_t> pairs[] = {
            {w.add_no_mpid, 'A'},       {w.add_with_mpid, 'F'}, {w.cancel, 'X'},
            {w.delete_order, 'D'},      {w.replace, 'U'},       {w.execute, 'E'},
            {w.execute_with_price, 'C'}};

        for (const auto& [weight, ty] : pairs) {
            if (r < weight) {
                return ty;
            }
            r -= weight;
        }
        return 'A';
    }

    [[nodiscard]] bool has_active_orders() const { return !active_ids_.empty(); }
    [[nodiscard]] std::size_t active_orders_count() const { return active_ids_.size(); }

private:
    static void w16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) {
        val = __builtin_bswap16(val);
        std::memcpy(buf.data() + offset, &val, 2);
    }
    static void w32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) {
        val = __builtin_bswap32(val);
        std::memcpy(buf.data() + offset, &val, 4);
    }
    static void w48(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) {
        val = __builtin_bswap64(val);
        std::memcpy(buf.data() + offset, reinterpret_cast<const char*>(&val) + 2, 6);
    }
    static void w64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) {
        val = __builtin_bswap64(val);
        std::memcpy(buf.data() + offset, &val, 8);
    }

    SyntheticConfig cfg_;
    std::mt19937_64 rng_;
    std::vector<ActiveOrder> orders_;
    std::vector<std::uint64_t> active_ids_;
    std::unordered_map<std::uint64_t, std::size_t> id_to_index_;
    std::uint64_t next_ref_{1};
    std::uint64_t next_timestamp_{9'000'000'000'000};
    std::uint64_t next_match_{1};
    std::uint16_t tracking_{1};
};

inline Feed generate(const SyntheticConfig& cfg) {
    Generator g(cfg);

    Feed feed;
    feed.prefill.reserve(cfg.prefill_orders);
    std::uniform_real_distribution<double> mpid_prob(0.0, 1.0);
    std::mt19937_64 prob_rng(cfg.seed + 1);

    for (std::size_t i = 0; i < cfg.prefill_orders; ++i) {
        bool with_mpid = (mpid_prob(prob_rng) < 0.05);
        feed.prefill.push_back(g.add(with_mpid));
    }

    feed.messages.reserve(cfg.message_count);
    for (std::size_t i = 0; i < cfg.message_count; ++i) {
        std::uint8_t ty = g.pick_type();
        if (!g.has_active_orders() && ty != 'A' && ty != 'F') {
            ty = 'A';
        }
        switch (ty) {
        case 'A':
            feed.messages.push_back(g.add(false));
            break;
        case 'F':
            feed.messages.push_back(g.add(true));
            break;
        case 'X':
            feed.messages.push_back(g.cancel());
            break;
        case 'D':
            feed.messages.push_back(g.delete_order());
            break;
        case 'U':
            feed.messages.push_back(g.replace());
            break;
        case 'E':
            feed.messages.push_back(g.execute(false));
            break;
        case 'C':
            feed.messages.push_back(g.execute(true));
            break;
        default:
            break;
        }
    }

    feed.final_active_orders = g.active_orders_count();
    return feed;
}

} // namespace bench::common::synthetic
