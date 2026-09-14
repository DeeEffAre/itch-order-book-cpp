#include "matching_engine/itch_parser/messages.hpp"
struct SlabIndex {
    std::uint32_t value;
    static const SlabIndex NULL_INDEX;

    explicit constexpr SlabIndex(std::uint32_t val) : value(val) {};

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return value == std::numeric_limits<std::uint32_t>::max();
    }

    constexpr bool operator<=>(const SlabIndex& other) const noexcept = default;
};

inline constexpr SlabIndex SlabIndex::NULL_INDEX{std::numeric_limits<std::uint32_t>::max()};

struct Id {
    std::uint64_t value;
    constexpr explicit Id(std::uint64_t val) noexcept : value(val) {}
    constexpr explicit Id(U64Be val) noexcept : value(val.get()) {}
    constexpr bool operator==(const Id& other) const noexcept = default;
};

struct Price {
    std::uint64_t value;
    constexpr explicit Price(std::uint64_t val) noexcept : value(val) {}
    constexpr explicit Price(U32Be val) noexcept : value(val.get()) {}
    constexpr bool operator==(const Price& other) const noexcept = default;
};

struct Quantity {
    std::uint32_t value;
    constexpr explicit Quantity(std::uint32_t val) noexcept : value(val) {}
    constexpr explicit Quantity(U32Be val) noexcept : value(val.get()) {}
    constexpr bool operator==(const Quantity& other) const noexcept = default;
};

struct Volume {
    std::uint32_t value = 0;

    constexpr Volume() = default;
    explicit constexpr Volume(std::uint32_t val) : value(val) {};
    constexpr bool operator<=>(const Volume& other) const noexcept = default;
    constexpr void add(Quantity shares) noexcept { value += shares.value; }
    constexpr void sub(Quantity shares) noexcept { value -= shares.value; }
};

// tick-based price offset
struct PriceOffset {
    std::uint64_t value = 0;
    static const PriceOffset NULL_INDEX;

    explicit constexpr PriceOffset(std::uint64_t val) : value(val) {};

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return value == std::numeric_limits<std::uint64_t>::max();
    }

    constexpr bool operator<=>(const PriceOffset& other) const noexcept = default;
};

inline constexpr PriceOffset PriceOffset::NULL_INDEX{std::numeric_limits<std::uint64_t>::max()};

struct Side {
    enum Type : std::uint8_t {
        BUY = 0,
        SELL = 1,
    };

    Type value;

    constexpr Side(Type t) noexcept : value(t) {}

    [[nodiscard]] static constexpr Side from_wire(std::uint8_t indicator) noexcept {
        return {indicator == 'B' ? BUY : SELL};
    }

    constexpr operator Type() const noexcept { return value; }
    constexpr bool operator==(Side other) const noexcept { return value == other.value; }
    constexpr bool operator==(Type other) const noexcept { return value == other; }
};
