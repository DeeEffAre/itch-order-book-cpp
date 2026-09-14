#pragma once

#include "matching_engine/itch_parser/messages.hpp"

#include <span>

template <typename MessageHandler> struct ItchParser {
    static void parse_and_dispatch(std::span<const uint8_t> raw, MessageHandler& handler) {
        if (raw.empty()) [[unlikely]] {
            return;
        }

        switch (raw[0]) {
        case 'S': {
            if (const SystemEventMessage* msg = SystemEventMessage::new_msg(raw)) [[likely]] {
                handler.on_system_event(msg);
            }
            break;
        }
        case 'A': {
            if (const AddOrderNoMPID* msg = AddOrderNoMPID::new_msg(raw)) [[likely]] {
                handler.on_add_order_no_mpid(msg);
            }
            break;
        }
        case 'F': {
            if (const AddOrderWithMPID* msg = AddOrderWithMPID::new_msg(raw)) [[likely]] {
                handler.on_add_order_with_mpid(msg);
            }
            break;
        }
        case 'X': {
            if (const OrderCancel* msg = OrderCancel::new_msg(raw)) [[likely]] {
                handler.on_order_cancel(msg);
            }
            break;
        }
        case 'D': {
            if (const OrderDelete* msg = OrderDelete::new_msg(raw)) [[likely]] {
                handler.on_order_delete(msg);
            }
            break;
        }
        case 'U': {
            if (const OrderReplace* msg = OrderReplace::new_msg(raw)) [[likely]] {
                handler.on_order_replace(msg);
            }
            break;
        }
        case 'E': {
            if (const OrderExecute* msg = OrderExecute::new_msg(raw)) [[likely]] {
                handler.on_order_execute(msg);
            }
            break;
        }
        case 'C': {
            if (const OrderExecuteWithPrice* msg = OrderExecuteWithPrice::new_msg(raw)) [[likely]] {
                handler.on_order_execute_with_price(msg);
            }
            break;
        }
        default:
            [[unlikely]] {
                break;
            }
        }
    }
};
