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
        [[nodiscard]] auto collect() -> tl::expected<Message, Error>;
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
        if (recv_ptr == encoded_recv_buffer.end())
        {
            reception_state = NO_MORE_SPACE;
            return;
        }
        *(recv_ptr++) = byte;
        if (byte == magic::stx && reception_state == IDLE)
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

    template<std::size_t MessageBufferSize>
    auto Receiver<MessageBufferSize>::collect() -> tl::expected<Message, Error>
    {
        if (reception_state == NO_MORE_SPACE) return tl::unexpected{Error::OUT_OF_MEMORY};
        if (!ready()) return tl::unexpected{Error::MESSAGE_NOT_READY};
        reset();

        auto encoded_view = encoded_data_view{encoded_recv_buffer};
        auto message_view = data_view{recv_buffer};
        auto truncated = message_t::truncate(encoded_view);

        if (!truncated) return tl::unexpected{truncated.error()};
        if (auto error = truncated->decode(message_view)) return tl::unexpected{error.value()};

        auto message = message_t::deserialize(message_view);
        if (!message) return tl::unexpected{message.error()};

        auto statically_typed = static_type(message.value());
        if (!statically_typed) return tl::unexpected{statically_typed.error()};

        return statically_typed.value();
    }

    template<std::size_t MessageBufferSize> void Receiver<MessageBufferSize>::reset()
    {
        recv_ptr = &encoded_recv_buffer[0];
        reception_state = IDLE;
    }
} // namespace mi