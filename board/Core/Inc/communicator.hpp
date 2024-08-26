#pragma once

#define MI_IMPLEMENT
#include "main.hpp"
#include "message_definitions.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <fstream>
#include <optional>
#include <syncstream>

namespace mi
{
    template<std::size_t MessageBufferSize> struct Communicator
    {
        explicit Communicator(std::ostream& stream_) : stream{stream_} {};

        template<std::convertible_to<Message> ConcreteMessage>
        [[nodiscard]] auto send(ConcreteMessage& message) -> std::optional<Error>;
        //        [[nodiscard]] auto recv() -> tl::expected<Message, Error>;
        //        [[nodiscard]] auto closed() -> bool;

    private:
        enum ReceptionState
        {
            IDLE,
            STX_RECEIVED,
            ETX_RECEIVED
        };

        std::array<uint8_t, MessageBufferSize> send_buffer;
        std::array<uint8_t, MessageBufferSize * 2> encoded_send_buffer;
        std::array<uint8_t, MessageBufferSize> recv_buffer;
        std::array<uint8_t, MessageBufferSize * 2> encoded_recv_buffer;
        uint8_t* recv_ptr = &encoded_recv_buffer[0];
        ReceptionState reception_state = IDLE;
        std::ostream& stream;
    };

    template<std::size_t MessageBufferSize>
    template<std::convertible_to<mi::Message> ConcreteMessage>
    auto Communicator<MessageBufferSize>::send(ConcreteMessage& message) -> std::optional<Error>
    {
        auto encoded_view = encoded_data_view{encoded_send_buffer};
        auto message_view = data_view{send_buffer};
        message_t msg = make_message(message);
        msg.serialize(message_view);
        std::optional<Error> error = message_view.encode(encoded_view);
        if (error) return error;
        std::basic_osyncstream{stream}.write(reinterpret_cast<char*>(encoded_view.data()),
                                             encoded_view.size());
        std::basic_osyncstream{stream} << std::flush;
        return std::nullopt;
    }

    //    template<std::size_t MessageBufferSize>
    //    auto Communicator<MessageBufferSize>::recv() -> tl::expected<Message, Error>
    //    {
    //        auto encoded_view = encoded_data_view{encoded_recv_buffer};
    //        auto message_view = data_view{recv_buffer};
    //        stream >> *recv_ptr;
    //        recv_ptr++;
    //        if (*recv_ptr == magic::stx && reception_state == ReceptionState::IDLE)
    //        {
    //            reception_state = ReceptionState::STX_RECEIVED;
    //        }
    //        if (*recv_ptr == magic::etx && reception_state == ReceptionState::STX_RECEIVED)
    //        {
    //            reception_state = ReceptionState::ETX_RECEIVED;
    //        }
    //        if (reception_state != ReceptionState::ETX_RECEIVED)
    //        {
    //            return tl::unexpected{Error::NO_ERROR};
    //        }
    //        recv_ptr = &encoded_recv_buffer[0];
    //        reception_state = ReceptionState::IDLE;
    //        tl::expected<encoded_data_view, Error> truncated = message_t::truncate(encoded_view);
    //        if (!truncated.has_value())
    //        {
    //            return tl::unexpected{truncated.error()};
    //        }
    //        if(std::optional<Error> error = truncated->decode(message_view))
    //        {
    //            return tl::unexpected{error.value()};
    //        }
    //        tl::expected<message_t, Error> message = message_t::deserialize(message_view);
    //        if (!message.has_value())
    //        {
    //            return tl::unexpected{message.error()};
    //        }
    //        return static_type(message.value());
    //    }

    //    template<std::size_t MessageBufferSize> auto Communicator<MessageBufferSize>::closed() ->
    //    bool
    //    {
    //        return stream.eof();
    //    }
} // namespace mi