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

    struct Ack
    {
        constexpr static uint16_t id = 1;

        uint16_t msg_id{UINT16_MAX};
        Error error;

        [[nodiscard]] constexpr auto operator==(const Ack&) const -> bool = default;
    };
    static_assert(sizeof(Ack) == 3, "'#pragma pack(push, 1)' didn't work or may not be supported");

    struct SetFrequencyData
    {
        constexpr static uint16_t id = 2;

        uint32_t min_freq = 1;
        float step_freq = 1.059F;

        [[nodiscard]] constexpr auto operator==(const SetFrequencyData&) const -> bool = default;
    };

    struct StartStreamingData
    {
        constexpr static uint16_t id = 3;

        uint32_t number_of_datums;

        [[nodiscard]] constexpr auto operator==(const StartStreamingData&) const -> bool = default;
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

    using Message = std::variant<Heartbeat, Ack, SetFrequencyData, StartStreamingData, FourierData>;

    [[nodiscard]] auto static_type(message_t& message) -> tl::expected<Message, Error>;
} // namespace mi
#ifdef MI_IMPLEMENT
namespace mi
{
    auto static_type(message_t& message) -> tl::expected<Message, Error>
    {
        // TODO: Some std::variant_alternative_t bs if I get the motivation to do it which I
        // probably won't
        switch (message.msg_id)
        {
        case Heartbeat::id: return message.payload.deserialize_into<Heartbeat>();
        case Ack::id: return message.payload.deserialize_into<Ack>();
        case SetFrequencyData::id: return message.payload.deserialize_into<SetFrequencyData>();
        case StartStreamingData::id: return message.payload.deserialize_into<StartStreamingData>();
        case FourierData::id: return message.payload.deserialize_into<FourierData>();
        default: return tl::unexpected{Error::UNKNOWN_MSG_ID};
        }
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