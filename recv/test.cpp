#define MI_IMPLEMENT
#include "fft.hpp"
#include "main.hpp"
#include "message_definitions.hpp"
#include "receiver.hpp"
#include "sender.hpp"
#include "to_string.hpp"

#include "tl-expected.hpp"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <list>
#include <numeric>
#include <ostream>
#include <string>

using namespace mi;

namespace mi
{
    auto operator<<(std::ostream& os, const data_view& view) -> std::ostream&
    {
        os << std::hex;
        for (auto c : view)
        {
            os << static_cast<uint32_t>(c) << ", ";
        }
        return os;
    }
} // namespace mi

#define ASSERT_ITERABLE_EQ(IT1, IT2)                                                               \
    do                                                                                             \
    {                                                                                              \
        ASSERT_EQ(IT1.size(), IT2.size());                                                         \
        const auto size = IT1.size();                                                              \
        for (std::size_t i = 0; i < size; ++i)                                                     \
        {                                                                                          \
            EXPECT_EQ(IT1[i], IT2[i]);                                                             \
        }                                                                                          \
    } while (false)

TEST(MiTest, ShouldSerializeAndDeserialize)
{
    Heartbeat heartbeat{1};
    message_t message = make_message(heartbeat);
    tl::expected<Heartbeat, Error> deserialized = message.payload.deserialize_into<Heartbeat>();

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(heartbeat.seq, deserialized->seq);
}

TEST(MiTest, ShouldSerializeAndDeserialize2)
{
    std::array<uint8_t, 7> amps{1, 2, 3, 4, 5, 6, 7};
    FourierData data{amps};
    message_t message = make_message(data);
    tl::expected<FourierData, Error> deserialized = message.payload.deserialize_into<FourierData>();

    ASSERT_TRUE(deserialized.has_value());
    ASSERT_ITERABLE_EQ(data.amplitudes, deserialized->amplitudes);
}

TEST(MiTest, ShouldEncodeAndDecode)
{
    std::array<uint8_t, 10> data;
    std::array<uint8_t, 20> encoded_storage;
    encoded_data_view encoded_storage_view{encoded_storage};
    std::array<uint8_t, 20> decoded_dest;
    data_view decoded_dest_view{decoded_dest};
    std::iota(data.begin(), data.end(), 245);

    data_view view{data};
    std::optional<Error> encode_error = view.encode(encoded_storage_view);
    ASSERT_FALSE(encode_error.has_value()) << static_cast<uint32_t>(encode_error.value());

    std::optional<Error> decode_error = encoded_storage_view.decode(decoded_dest_view);
    ASSERT_FALSE(decode_error.has_value()) << static_cast<uint32_t>(decode_error.value());

    for (std::size_t i = 0; i < data.size(); ++i)
    {
        EXPECT_EQ(data[i], decoded_dest[i]) << "Difference at index: " << i;
    }
}

TEST(MiTest, ShouldTrucateData)
{
    std::array data{uint8_t{0},
                    uint8_t{magic::stx},
                    uint8_t{1},
                    uint8_t{magic::stx},
                    uint8_t{2},
                    uint8_t{3},
                    uint8_t{4},
                    uint8_t{magic::etx},
                    uint8_t{5},
                    uint8_t{6},
                    uint8_t{magic::etx},
                    uint8_t{7}};
    encoded_data_view view{data};
    encoded_data_view expected{&data[3], 5};
    tl::expected<encoded_data_view, Error> truncated = message_t::truncate(view);
    ASSERT_TRUE(truncated.has_value()) << static_cast<uint32_t>(truncated.error());
    EXPECT_EQ(truncated.value(), expected);
}

struct MessageDeserializationTest :
    testing::TestWithParam<std::pair<data_view, tl::expected<message_t, Error>>>
{
    static auto repr(tl::expected<message_t, Error> expected) -> std::string
    {
        if (expected.has_value())
        {
            std::ostringstream oss;
            oss << "msg_id: " << expected->msg_id << " crc: " << expected->crc
                << " payload: " << expected->payload;
            return oss.str();
        }
        return "Error: " + std::to_string(static_cast<unsigned>(expected.error()));
    }
};

TEST_P(MessageDeserializationTest, ShouldDeserialize)
{
    const auto& [view, expected] = GetParam();
    const auto deserialized = message_t::deserialize(view);
    EXPECT_EQ(deserialized, expected)
        << "Expected:\n\t" << repr(expected) << "\nGot:\n\t" << repr(deserialized);
}

template<std::size_t Sz> auto static_data_view(const uint8_t (&in)[Sz]) -> data_view
{
    std::array<uint8_t, Sz> tmp;
    std::copy_n(in, Sz, tmp.begin());
    static std::list<std::array<uint8_t, Sz>> vec;
    return data_view{vec.emplace_back(tmp)};
}

template<std::size_t Sz> auto static_data_view_with_crc(const uint8_t (&in)[Sz]) -> data_view
{
    std::array<uint8_t, Sz - 1> tmp;
    std::copy_n(in, Sz - 1, tmp.begin());
    static std::list<std::array<uint8_t, Sz + 2>> vec;
    auto& data = vec.emplace_back();
    std::copy(tmp.begin(), tmp.end(), data.begin());
    auto crc = crc16(data_view{tmp});
    data[data.size() - 3] = crc & UINT8_MAX;
    data[data.size() - 2] = crc >> UINT8_WIDTH;
    data.back() = in[Sz - 1];
    return data_view{data};
}

std::vector<MessageDeserializationTest::ParamType> message_deserialization_test_cases{
    {static_data_view({0}), tl::unexpected{Error::NOT_ENOUGH_DATA}},
    {static_data_view({0, 1, 2, 3, 4, 5}), tl::unexpected{Error::INVALID_MAGIC}},
    {static_data_view_with_crc({0, 0, 0, magic::etx}), tl::unexpected{Error::INVALID_MAGIC}},
    {static_data_view_with_crc({magic::stx, 0, 0, 0}), tl::unexpected{Error::INVALID_MAGIC}},
    {static_data_view_with_crc({magic::stx, 0, 0, magic::etx}),
     message_t{
         .msg_id = 0,
         .payload = data_view{},
         .crc = 28063, // Written by hand
     }},
    {static_data_view_with_crc({magic::stx, 0, 0, 1, 2, 3, magic::etx}),
     message_t{
         .msg_id = 0,
         .payload = static_data_view({1, 2, 3}),
         .crc = 45278, // Written by hand
     }},
};

INSTANTIATE_TEST_SUITE_P(,
                         MessageDeserializationTest,
                         testing::ValuesIn(message_deserialization_test_cases));

TEST(MessageSerializationTest, ShouldSerialize)
{
    std::array<uint8_t, 5> payload{1, 2, 3, 4, 5};
    message_t message{
        .msg_id = 4,
        .payload = data_view{payload},
    };
    std::array<uint8_t, 20> serialized_storage{};
    data_view serialized{serialized_storage};
    message.serialize(serialized);

    tl::expected<message_t, Error> deserialized = message_t::deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value()) << static_cast<unsigned>(deserialized.error());
    EXPECT_EQ(deserialized->msg_id, message.msg_id);
    EXPECT_EQ(deserialized->payload, message.payload);
}

template<typename... Callables> struct OverloadSet : Callables...
{
    OverloadSet(const Callables&...){};
    using Callables::operator()...;
};

struct MessageStaticTypingTest :
    testing::TestWithParam<std::pair<message_t, tl::expected<Message, Error>>>
{
    static auto repr(const tl::expected<Message, Error>& expected) -> std::string
    {
        if (expected.has_value())
        {
            return mi::to_string(expected.value());
        }
        return "Error: " + std::to_string(static_cast<uint32_t>(expected.error()));
    }
};

TEST_P(MessageStaticTypingTest, ShouldStaticallyType)
{
    auto [message, expected] = GetParam();

    tl::expected<Message, Error> static_message = static_type(message);
    EXPECT_EQ(expected, static_message)
        << "Expected:\n\t" << repr(expected) << "\nGot:\n\t" << repr(static_message);
}

template<std::size_t N> auto static_span(const uint8_t (&in)[N])
{
    static std::list<std::array<uint8_t, N>> store;
    auto& new_store = store.emplace_back();
    std::copy_n(in, N, new_store.begin());
    return std::span<uint8_t>{new_store};
}

std::vector<MessageStaticTypingTest::ParamType> message_static_typing_test_cases{
    {
        message_t{Heartbeat::id, static_data_view({1})},
        Heartbeat{1},
    },
    {
        message_t{Ack::id, static_data_view({2, 0, 2})},
        Ack{SetFrequencyData::id, Error::INCORRECT_ALIGNMENT},
    },
    {
        message_t{SetFrequencyData::id, static_data_view({1, 0, 0, 0, 0, 0, 0, 63})},
        SetFrequencyData{1, 0.5F},
    },
    {
        message_t{StartStreamingData::id, {static_data_view({1, 0, 0, 0})}},
        StartStreamingData{1},
    },
    {
        message_t{FourierData::id, static_data_view({1, 2})},
        FourierData{static_span({uint8_t{1}, uint8_t{2}})},
    },
};

INSTANTIATE_TEST_SUITE_P(,
                         MessageStaticTypingTest,
                         testing::ValuesIn(message_static_typing_test_cases));

TEST(ReceiverTest, ShouldReceive)
{
    std::array<uint8_t, 7> data{0xfd, 0x00, 0x00, 0x00, 11, 34, 0xfe};
    Receiver<20> receiver;
    for (auto c : data)
    {
        receiver.put(c);
    }
    ASSERT_TRUE(receiver.ready());
    auto collected = receiver.collect();
    ASSERT_TRUE(collected.has_value()) << static_cast<uint32_t>(collected.error());
    EXPECT_EQ(std::get<Heartbeat::id>(collected.value()), Heartbeat{0});
}

class SenderTest : public testing::TestWithParam<Message>
{
protected:
    void TearDown() override { out.clear(); }
    static inline std::vector<uint8_t> out;
};

TEST_P(SenderTest, ShouldSend)
{
    Message msg = GetParam();
    Sender<100> sender{([](uint8_t byte) { out.push_back(byte); })};
    Receiver<100> receiver;
    std::optional<Error> error = std::visit([&sender](auto m) { return sender.send(m); }, msg);
    EXPECT_FALSE(error.has_value()) << static_cast<uint32_t>(error.value());
    for (uint8_t c : out)
    {
        receiver.put(c);
    }
    ASSERT_TRUE(receiver.ready());
    tl::expected<Message, Error> collected = receiver.collect();
    ASSERT_TRUE(collected.has_value()) << static_cast<uint32_t>(collected.error());
    EXPECT_EQ(msg, collected.value());
}

std::vector<Message> sender_test_cases{
    Heartbeat{0},
    SetFrequencyData{1, 0.5F},
    FourierData{static_span({uint8_t{1}, uint8_t{2}})},
};

INSTANTIATE_TEST_SUITE_P(, SenderTest, testing::ValuesIn(sender_test_cases));