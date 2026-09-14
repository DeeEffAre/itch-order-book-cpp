#pragma once

#include <cstring>
#pragma pack(1)

struct U16Be {
    std::array<uint8_t, 2> raw_;

    [[nodiscard]] constexpr uint16_t get() const noexcept {
        uint16_t val{};
        std::memcpy(&val, raw_.data(), 2);
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }

    constexpr operator uint16_t() const noexcept { return get(); }
};

struct U32Be {
    std::array<uint8_t, 4> raw_;

    [[nodiscard]] constexpr uint32_t get() const noexcept {
        uint32_t val{};
        std::memcpy(&val, raw_.data(), 4);
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }

    constexpr operator uint32_t() const noexcept { return get(); }
};

struct U48Be {
    std::array<uint8_t, 6> raw_;

    [[nodiscard]] constexpr uint64_t get() const noexcept {
        // Rust: u64::from_be_bytes([0, 0, b[0], b[1], b[2], b[3], b[4], b[5]])
        return (static_cast<uint64_t>(raw_[0]) << 40) | (static_cast<uint64_t>(raw_[1]) << 32) |
               (static_cast<uint64_t>(raw_[2]) << 24) | (static_cast<uint64_t>(raw_[3]) << 16) |
               (static_cast<uint64_t>(raw_[4]) << 8) | (static_cast<uint64_t>(raw_[5]));
    }

    constexpr operator uint64_t() const noexcept { return get(); }
};

struct U64Be {
    std::array<uint8_t, 8> raw_;

    [[nodiscard]] constexpr uint64_t get() const noexcept {
        uint64_t val{};
        std::memcpy(&val, raw_.data(), 8);
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }

    constexpr operator uint64_t() const noexcept { return get(); }
};

template <typename Derived> struct Message {
private:
    Message() = default;

public:
    [[nodiscard]] static const Derived* new_msg(std::span<const uint8_t> raw) noexcept {
        if (raw.size() < sizeof(Derived)) [[unlikely]] {
            return nullptr;
        }
        return reinterpret_cast<const Derived*>(raw.data());
    }

    // std::byte overload'u
    [[nodiscard]] static const Derived* new_msg(std::span<const std::byte> raw) noexcept {
        if (raw.size() < sizeof(Derived)) [[unlikely]] {
            return nullptr;
        }
        return reinterpret_cast<const Derived*>(raw.data());
    }

    // std::array için doğrudan overload
    template <std::size_t N>
    [[nodiscard]] static const Derived* new_msg(const std::array<uint8_t, N>& raw) noexcept {
        static_assert(N >= sizeof(Derived), "Buffer size is smaller than message size");
        return reinterpret_cast<const Derived*>(raw.data());
    }
    friend Derived;
};

struct SystemEventMessage : Message<SystemEventMessage> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    uint8_t event_code;
};

struct AddOrderNoMPID : Message<AddOrderNoMPID> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
    uint8_t indicator;
    U32Be shares;
    U64Be stock;
    U32Be price;
};

struct AddOrderWithMPID : Message<AddOrderWithMPID> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
    uint8_t indicator;
    U32Be shares;
    U64Be stock;
    U32Be price;
    U32Be attribution;
};

struct OrderCancel : Message<OrderCancel> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
    U32Be cancelled_shares;
};

struct OrderDelete : Message<OrderDelete> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
};

struct OrderExecute : Message<OrderExecute> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
    U32Be executed_shares;
    U64Be match_numbers;
};

struct OrderExecuteWithPrice : Message<OrderExecuteWithPrice> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be order_ref_number;
    U32Be executed_shares;
    U64Be match_numbers;
    uint8_t printable;
    U32Be execution_price;
};

struct OrderReplace : Message<OrderReplace> {
    uint8_t message_type;
    U16Be stock_locate;
    U16Be tracking_number;
    U48Be timestamp;
    U64Be org_order_ref_number;
    U64Be new_order_ref_number;
    U32Be shares;
    U32Be price;
};

#pragma pack()

static_assert(sizeof(SystemEventMessage) == 12 && alignof(SystemEventMessage) == 1);
static_assert(sizeof(AddOrderNoMPID) == 36 && alignof(AddOrderNoMPID) == 1);
static_assert(sizeof(AddOrderWithMPID) == 40 && alignof(AddOrderWithMPID) == 1);
static_assert(sizeof(OrderCancel) == 23 && alignof(OrderCancel) == 1);
static_assert(sizeof(OrderDelete) == 19 && alignof(OrderDelete) == 1);
static_assert(sizeof(OrderExecute) == 31 && alignof(OrderExecute) == 1);
static_assert(sizeof(OrderExecuteWithPrice) == 36 && alignof(OrderExecuteWithPrice) == 1);
static_assert(sizeof(OrderReplace) == 35 && alignof(OrderReplace) == 1);
