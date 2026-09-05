#pragma once

struct __attribute__((packed)) SystemEventMessage {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    uint8_t /*          */ event_code_;
};

struct __attribute__((packed)) AddOrderNoMPID {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
    uint8_t /*          */ indicator_;
    std::array<uint8_t, 4> shares_;
    std::array<uint8_t, 8> stock_;
    std::array<uint8_t, 4> price_;
};

struct __attribute__((packed)) AddOrderWithMPID {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
    uint8_t /*          */ indicator_;
    std::array<uint8_t, 4> shares_;
    std::array<uint8_t, 8> stock_;
    std::array<uint8_t, 4> price_;
    std::array<uint8_t, 4> attribution_;
};

struct __attribute__((packed)) OrderCancel {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
    std::array<uint8_t, 4> cancelled_shares_;
};

struct __attribute__((packed)) OrderDelete {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
};

struct __attribute__((packed)) OrderExecute {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
    std::array<uint8_t, 4> executed_shares_;
    std::array<uint8_t, 8> match_numbers_;
};

struct __attribute__((packed)) OrderExecuteWithPrice {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> order_ref_number_;
    std::array<uint8_t, 4> executed_shares_;
    std::array<uint8_t, 8> match_numbers_;
    uint8_t /*          */ printable_;
    std::array<uint8_t, 4> execution_price_;
};

struct __attribute__((packed)) OrderReplace {
    uint8_t /*          */ message_type_;
    std::array<uint8_t, 2> stock_locate_;
    std::array<uint8_t, 2> tracking_number_;
    std::array<uint8_t, 6> timestamp_;
    std::array<uint8_t, 8> org_order_ref_number_;
    std::array<uint8_t, 8> new_order_ref_number_;
    std::array<uint8_t, 4> shares_;
    std::array<uint8_t, 4> price_;
};
