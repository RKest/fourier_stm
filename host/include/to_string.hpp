#pragma once

#include "message_definitions.hpp"
#include <fmt/format.h>

namespace mi
{
    auto to_string(const Message& message) -> std::string
    {
        return std::visit(
            OverloadSet{
                [](Heartbeat hb) -> std::string
                { return fmt::format("Heartbeat(.seq = {})", hb.seq); },
                [](Ack ack) -> std::string
                {
                    return fmt::format("Ack(.msg_id = {}, .error = {})",
                                       ack.msg_id,
                                       static_cast<uint32_t>(ack.error));
                },
                [](SetFrequencyData data) -> std::string
                {
                    return fmt::format("SetFrequencyData(.min_freq = {}, .step_freq = {})",
                                       data.min_freq,
                                       data.step_freq);
                },
                [](StartStreamingData data) -> std::string {
                    return fmt::format("StartStreamingData(.number_of_datums = {})",
                                       data.number_of_datums);
                },
                [](FourierData data) -> std::string {
                    return fmt::format("FourierData(.amplitudes = {})",
                                       fmt::join(data.amplitudes, ", "));
                },
                [](auto other) -> std::string { return "Unknown"; },
            },
            message);
    }
} // namespace mi