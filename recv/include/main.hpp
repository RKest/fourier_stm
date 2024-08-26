#pragma once

#include "tl-expected.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <ostream>

namespace mi
{
#ifndef UINT8_WIDTH
#    define UINT8_WIDTH 8
#endif
    template<typename... Callables> struct OverloadSet : Callables...
    {
        OverloadSet(const Callables&...){};
        using Callables::operator()...;
    };

    enum struct Error : uint8_t
    {
        NO_ERROR,            // 0
        INVALID_MAGIC,       // 1
        INCORRECT_ALIGNMENT, // 2
        NOT_ENOUGH_DATA,     // 3
        INVALID_CRC,         // 4
        INCOMPLETE,          // 5
        UNKNOWN_MSG_ID,      // 6
        OUT_OF_MEMORY,       // 7
        MESSAGE_NOT_READY,   // 8
    };

    struct data_view;
    struct encoded_data_view;

    template<typename T>
    concept explicitly_deserializable = requires(data_view& data) {
        {
            T::deserialize(data)
        } -> std::same_as<tl::expected<T, Error>>;
    };

    template<typename T>
    concept explicitly_serializable = requires(T& type) {
        {
            type.serialize()
        } -> std::same_as<data_view>;
    };

    template<typename T>
    concept trivially_copyable = std::is_trivially_copyable_v<T>;

    template<typename T>
    concept implicitly_serializable = trivially_copyable<T> && !explicitly_serializable<T>;
    template<typename T>

    concept implicitly_deserializable = trivially_copyable<T> && !explicitly_deserializable<T>;

    struct data_view
    {
        template<std::size_t Size>
        constexpr explicit data_view(std::array<uint8_t, Size>& data) :
            ptr{data.data()}, length{data.size()}
        {
        }
        constexpr data_view(uint8_t* ptr_, uintptr_t length_) : ptr{ptr_}, length{length_} {}
        constexpr data_view() = default;
        [[nodiscard]] constexpr auto operator==(const data_view& other) const -> bool
        {
            return size() == other.size() && std::equal(begin(), end(), other.begin());
        }

        [[nodiscard]] constexpr auto begin() const noexcept -> const uint8_t* { return ptr; }
        [[nodiscard]] constexpr auto end() const noexcept -> const uint8_t* { return ptr + length; }
        [[nodiscard]] constexpr auto begin() noexcept -> uint8_t* { return ptr; }
        [[nodiscard]] constexpr auto end() noexcept -> uint8_t* { return ptr + length; }
        [[nodiscard]] constexpr auto front() const noexcept -> std::uint8_t& { return *ptr; }
        [[nodiscard]] constexpr auto back() const noexcept -> std::uint8_t&
        {
            return *(ptr + length - 1);
        }
        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return length; }
        [[nodiscard]] constexpr auto data() const noexcept -> std::uint8_t const* { return ptr; };
        [[nodiscard]] constexpr auto data() noexcept -> std::uint8_t* { return ptr; };
        [[nodiscard]] constexpr auto at(std::size_t i) const noexcept -> uint8_t
        {
            return *(ptr + i);
        }
        constexpr void resize(std::size_t new_size) noexcept { length = new_size; }
        constexpr void remove_prefix(std::uintptr_t len) noexcept
        {
            ptr += len;
            length -= len;
        };
        constexpr void remove_postfix(std::uintptr_t len) noexcept { length -= len; }
        [[nodiscard]] constexpr auto take_one() noexcept -> uint8_t
        {
            auto curr = front();
            remove_prefix(1);
            return curr;
        };
        [[nodiscard]] constexpr auto take_two() noexcept -> uint16_t
        {
            auto first = take_one();
            auto second = take_one();
            return (static_cast<uint16_t>(second) << UINT8_WIDTH) + first;
        }
        [[nodiscard]] constexpr auto drop_one() noexcept -> uint8_t
        {
            auto curr = back();
            remove_postfix(1);
            return curr;
        };
        [[nodiscard]] constexpr auto drop_two() noexcept -> uint16_t
        {
            auto second = drop_one();
            auto first = drop_one();
            return (static_cast<uint16_t>(second) << UINT8_WIDTH) + first;
        };
        template<implicitly_deserializable Type>
        [[nodiscard]] auto deserialize_into() noexcept -> tl::expected<Type, Error>;
        template<explicitly_deserializable Type>
        [[nodiscard]] auto deserialize_into() noexcept -> tl::expected<Type, Error>;
        template<implicitly_serializable Type>
        [[nodiscard]] static auto from_serialized(Type& type) noexcept -> data_view;
        template<explicitly_serializable Type>
        [[nodiscard]] static auto from_serialized(Type& type) noexcept -> data_view;

        [[nodiscard]] auto encode(encoded_data_view& dest) noexcept -> std::optional<Error>;

    private:
        std::uint8_t* ptr;
        std::uintptr_t length;
    };

    struct encoded_data_view : private data_view
    {
        using data_view::begin;
        using data_view::data;
        using data_view::data_view;
        using data_view::end;
        using data_view::front;
        using data_view::resize;
        using data_view::size;

        friend auto operator<<(std::ostream& os, const encoded_data_view& view) -> std::ostream&;
        [[nodiscard]] constexpr auto operator==(const encoded_data_view& other) const -> bool
        {
            return size() == other.size() && std::equal(begin(), end(), other.begin());
        }
        [[nodiscard]] auto decode(data_view& dest) -> std::optional<Error>;
    };

    struct message_t
    {
        constexpr static auto max_payload_len = std::numeric_limits<uint16_t>::max();

        uint16_t msg_id;
        data_view payload;
        uint16_t crc;

        [[nodiscard]] constexpr auto operator==(const message_t&) const -> bool = default;
        [[nodiscard]] static auto truncate(encoded_data_view)
            -> tl::expected<encoded_data_view, Error>;
        [[nodiscard]] static auto deserialize(data_view data) -> tl::expected<message_t, Error>;
        void serialize(mi::data_view& dest) const;
    };

    template<typename Message> [[nodiscard]] auto make_message(Message& typed_message) -> message_t
    {
        message_t message;
        message.msg_id = Message::id;
        message.payload = data_view::from_serialized(typed_message);
        return message;
    }

    template<implicitly_deserializable Type>
    auto data_view::deserialize_into() noexcept -> tl::expected<Type, Error>
    {
        if (sizeof(Type) != size()) return tl::unexpected{Error::INCORRECT_ALIGNMENT};

        Type out;
        std::memcpy(&out, data(), sizeof(Type));
        return out;
    }

    template<explicitly_deserializable Type>
    auto data_view::deserialize_into() noexcept -> tl::expected<Type, Error>
    {
        return Type::deserialize(*this);
    }

    template<implicitly_serializable Type>
    auto data_view::from_serialized(Type& type) noexcept -> data_view
    {
        return data_view{reinterpret_cast<uint8_t*>(&type), sizeof(Type)};
    }

    template<explicitly_serializable Type>
    auto data_view::from_serialized(Type& type) noexcept -> data_view
    {
        return type.serialize();
    }

    namespace magic
    {
        constexpr static uint8_t stx = 0xFD;
        constexpr static uint8_t etx = 0xFE;
        constexpr static uint8_t encoder = 0xFC;
    } // namespace magic

#ifdef MI_IMPLEMENT
    auto operator<<(std::ostream& os, const encoded_data_view& view) -> std::ostream&
    {
        for (auto c : view)
        {
            os << c;
        }
        return os;
    }

    [[nodiscard]] auto crc16(data_view data) -> uint16_t
    {
        uint8_t x;
        uint16_t crc = 0xFFFF;
        for (auto c : data)
        {
            x = crc >> 8 ^ c;
            x ^= x >> 4;
            // clang-format off
            crc = (crc << 8)
                  ^ (static_cast<uint16_t>(x << 12))
                  ^ (static_cast<uint16_t>(x << 5))
                  ^ (static_cast<uint16_t>(x));
            // clang-format on
        }
        return crc;
    }

    auto data_view::encode(encoded_data_view& dest) noexcept -> std::optional<Error>
    {
        constexpr auto pessimistic_encoding_size = 2;
        if (dest.size() < size() * pessimistic_encoding_size) return Error::NOT_ENOUGH_DATA;
        dest.resize(0);
        auto append = [&dest, head = dest.data()](uint8_t byte) mutable
        {
            *(head++) = byte;
            dest.resize(dest.size() + 1);
        };

        auto encode_case = [&append, &dest](uint8_t special_char)
        {
            append(magic::encoder);
            append(special_char - magic::encoder);
        };

        for (auto c : *this)
        {
            switch (c)
            {
            case magic::etx: [[fallthrough]];
            case magic::stx: [[fallthrough]];
            case magic::encoder: encode_case(c); break;
            default: append(c);
            }
        }
        return std::nullopt;
    }

    auto encoded_data_view::decode(mi::data_view& dest) -> std::optional<Error>
    {
        if (dest.size() < size()) return Error::NOT_ENOUGH_DATA;
        dest.resize(0);

        auto append = [&dest, head = dest.data()](uint8_t byte) mutable
        {
            *(head++) = byte;
            dest.resize(dest.size() + 1);
        };

        bool should_encode = false;
        for (auto c : *this)
        {
            if (c == magic::encoder)
            {
                should_encode = true;
                continue;
            }
            append(magic::encoder * should_encode + c);
            should_encode = false;
        }
        return std::nullopt;
    }

    auto message_t::truncate(mi::encoded_data_view data) -> tl::expected<encoded_data_view, Error>
    {
        uint8_t* result_start = nullptr;
        uintptr_t result_len = UINTPTR_MAX;
        for (auto& c : data)
        {
            if (c == magic::stx)
            {
                result_start = &c;
            }
            if (result_start != nullptr && c == magic::etx)
            {
                result_len = std::distance(result_start, &c) + 1;
                break;
            }
        }
        if (std::distance(result_start, data.end()) < result_len)
            return tl::unexpected{Error::NOT_ENOUGH_DATA};
        if (result_len == UINTPTR_MAX) return tl::unexpected{Error::INCOMPLETE};

        return encoded_data_view{result_start, result_len};
    }

    constexpr static auto min_message_t_len = sizeof(magic::stx) + sizeof(magic::etx)
                                              + sizeof(message_t::msg_id) + sizeof(message_t::crc);
    auto message_t::deserialize(data_view data) -> tl::expected<message_t, Error>
    {
        message_t result;
        if (data.size() < min_message_t_len) return tl::unexpected{Error::NOT_ENOUGH_DATA};
        if (data.drop_one() != magic::etx) return tl::unexpected{Error::INVALID_MAGIC};
        result.crc = data.drop_two();
        auto new_crc = crc16(data);
        if (data.take_one() != magic::stx) return tl::unexpected{Error::INVALID_MAGIC};
        if (result.crc != new_crc) return tl::unexpected{Error::INVALID_CRC};
        result.msg_id = data.take_two();
        result.payload = data;
        return result;
    }

    auto message_t::serialize(mi::data_view& dest) const -> void
    {
        auto append = [head = dest.data()](uint8_t byte) mutable
        {
            *(head++) = byte;
        };
        append(magic::stx);
        append(msg_id & UINT8_MAX);
        append(msg_id >> UINT8_WIDTH);
        for (auto c : payload)
            append(c);
        dest.resize(payload.size() + min_message_t_len - sizeof(message_t::crc) - sizeof(magic::etx));
        auto new_crc = crc16(dest);
        append(new_crc & UINT8_MAX);
        append(new_crc >> UINT8_WIDTH);
        append(magic::etx);
        dest.resize(payload.size() + min_message_t_len);
    }
#endif
} // namespace mi