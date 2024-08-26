#pragma once

#include "message_definitions.hpp"

#include <array>
#include <cstdint>
#include <tl-expected.hpp>

namespace mi
{
    template<std::size_t MessageBufferSize> struct Receiver
    {
        void put(uint8_t byte);
        [[nodiscard]] auto collect(Message& out) -> std::optional<Error>;
        [[nodiscard]] auto ready() -> bool { return reception_state == ETX_RECEIVED; }

    private:
        void reset();

        enum ReceptionState
        {
            IDLE,
            STX_RECEIVED,
            ETX_RECEIVED,
            NO_MORE_SPACE
        };

        std::array<uint8_t, MessageBufferSize> recv_buffer;
        std::array<uint8_t, MessageBufferSize> encoded_recv_buffer;
        uint8_t* recv_ptr = &encoded_recv_buffer[0];
        ReceptionState reception_state = IDLE;
    };

    template<std::size_t MessageBufferSize> void Receiver<MessageBufferSize>::put(uint8_t byte)
    {
        *(recv_ptr++) = byte;
        if (byte == magic::stx)
        {
            reception_state = STX_RECEIVED;
            return;
        }
        if (byte == magic::etx && reception_state == STX_RECEIVED)
        {
            reception_state = ETX_RECEIVED;
            return;
        }
    }

    // Out parameter is because it's too much for STM32 to handle double type erasure lol
    template<std::size_t MessageBufferSize>
    auto Receiver<MessageBufferSize>::collect(Message& out) -> std::optional<Error>
    {
        if (!ready()) return Error::MESSAGE_NOT_READY;
        reset();

        auto encoded_view = encoded_data_view{encoded_recv_buffer};
        auto message_view = data_view{recv_buffer};
        auto truncated = message_t::truncate(encoded_view);

        if (!truncated) return truncated.error();
        if (auto error = truncated->decode(message_view)) return error.value();

        auto message = message_t::deserialize(message_view);
        if (!message) return message.error();

        auto [statically_typed, error] = static_type(message.value());
        if (error) return error;
    	out = statically_typed;

        return std::nullopt;
    }

    template<std::size_t MessageBufferSize> void Receiver<MessageBufferSize>::reset()
    {
        recv_ptr = &encoded_recv_buffer[0];
        reception_state = IDLE;
    }
} // namespace mi
