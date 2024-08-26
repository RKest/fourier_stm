#pragma once

#include "main.hpp"

#include <span>
#include <variant>

namespace mi
{
#pragma pack(push, 1)
    struct Heartbeat
    {
        constexpr static uint16_t id = 0;

        uint8_t seq;

        [[nodiscard]] constexpr auto operator==(const Heartbeat&) const -> bool = default;
    };
#pragma pack(pop)

    struct FourierData
    {
        constexpr static uint16_t id = 4;

        explicit FourierData(std::span<uint8_t> amplitudes_);

        [[nodiscard]] static auto deserialize(data_view& data) -> tl::expected<FourierData, Error>;
        [[nodiscard]] auto serialize() -> data_view;

        std::span<uint8_t> amplitudes;

        [[nodiscard]] constexpr auto operator==(const FourierData& other) const -> bool
        {
            return amplitudes.size() == other.amplitudes.size()
                   && std::equal(amplitudes.begin(), amplitudes.end(), other.amplitudes.begin());
        }
    };

    static_assert(explicitly_serializable<FourierData>);
    static_assert(explicitly_deserializable<FourierData>);

    using Message = std::variant<Heartbeat, FourierData>;

    [[nodiscard]] auto static_type(message_t& message) -> std::pair<Message, std::optional<Error>>;
} // namespace mi
#ifdef MI_IMPLEMENT
namespace mi
{
    auto static_type(message_t& message) -> std::pair<Message, std::optional<Error>>
    {
        // TODO: Some std::variant_alternative_t bs if I get the motivation to do it which I
        // probably won't
    	Message out;
    	std::optional<Error> err;
        switch (message.msg_id)
        {
        case Heartbeat::id:
        {
        	auto msg = message.payload.deserialize_into<Heartbeat>();
        	if (msg) out = *msg;
        	else err = msg.error();
        	break;
        }
        case FourierData::id:
        {
        	auto msg = message.payload.deserialize_into<FourierData>();
        	if (msg) out = *msg;
        	else err = msg.error();
        	break;
        }
        default: err = Error::UNKNOWN_MSG_ID;
        }
        return {out, err};
    }

    FourierData::FourierData(std::span<uint8_t> amplitudes_) : amplitudes{amplitudes_} {}

    auto FourierData::deserialize(mi::data_view& data) -> tl::expected<FourierData, Error>
    {
        return FourierData{{
            data.begin(),
            data.size(),
        }};
    }

    auto FourierData::serialize() -> data_view
    {
        return {
            amplitudes.data(),
            amplitudes.size(),
        };
    }
} // namespace mi
#endif
