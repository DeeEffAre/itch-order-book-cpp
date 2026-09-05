#pragma once

#include "matching_engine/itch_parser/messages.hpp"

template <typename MessageHandler> struct ItchParser {

    static void parse_and_dispatch(std::span<const std::byte> raw, MessageHandler& handler) {
        if (raw.empty()) [[unlikely]] {
            return;
        }

        switch (static_cast<char>(raw[0])) {
        case 'S': {
            const auto* msg = reinterpret_cast<const SystemEventMessage*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'A': {
            const auto* msg = reinterpret_cast<const AddOrderNoMPID*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'F': {
            const auto* msg = reinterpret_cast<const AddOrderWithMPID*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'X': {
            const auto* msg = reinterpret_cast<const OrderCancel*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'D': {
            const auto* msg = reinterpret_cast<const OrderDelete*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'U': {
            const auto* msg = reinterpret_cast<const OrderReplace*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'E': {
            const auto* msg = reinterpret_cast<const OrderExecute*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        case 'C': {
            const auto* msg = reinterpret_cast<const OrderExecuteWithPrice*>(raw.data());
            handler.on_message(*msg);
            break;
        }
        default:
            [[unlikely]] {
                break;
            }
        }
    }
};
