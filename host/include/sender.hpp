#pragma once

#include "message_definitions.hpp"

namespace mi
{
    template<std::size_t MessageBufferSize> struct Sender
    {
        using CallbackType = void (*)(uint8_t);

        Sender(CallbackType callback_) : callback{callback_} {};
        template<std::convertible_to<Message> ConcreteMessage>
        [[nodiscard]] auto send(ConcreteMessage& message) -> std::optional<Error>;

    private:
        auto encode_message(data_view& view) -> tl::expected<encoded_data_view, Error>;

        CallbackType callback;

        std::array<uint8_t, MessageBufferSize> send_buffer;
        std::array<uint8_t, MessageBufferSize * 2> encoded_send_buffer;
    };

    template<std::size_t MessageBufferSize>
    template<std::convertible_to<mi::Message> ConcreteMessage>
    auto Sender<MessageBufferSize>::send(ConcreteMessage& message) -> std::optional<Error>
    {
        auto message_view = data_view{send_buffer};
        message_t msg = make_message(message);
        msg.serialize(message_view);
        auto encoded_view = encode_message(message_view);
        if (!encoded_view.has_value()) return encoded_view.error();
        for (auto byte : *encoded_view)
        {
            callback(byte);
        }
        return std::nullopt;
    }

    template<std::size_t MessageBufferSize>
    auto Sender<MessageBufferSize>::encode_message(data_view& view)
        -> tl::expected<encoded_data_view, Error>
    {
        encoded_send_buffer[0] = magic::stx;
        encoded_data_view encoded{encoded_send_buffer.begin() + 1, encoded_send_buffer.size() - 1};
        data_view to_encode_chunk{view.begin() + 1, view.size() - 4};
        std::optional<Error> error = to_encode_chunk.encode(encoded);
        if (error) return tl::make_unexpected(error.value());
        encoded.resize(encoded.size() + 3);
        std::copy(view.end() - 3, view.end(), encoded.end() - 3);
        return encoded_data_view{&encoded_send_buffer[0], encoded.size() + 1};
    }
} // namespace mi