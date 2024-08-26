#pragma once

#define __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
#include "message_definitions.hpp"
#include "simple_fft/fft.hpp"
#include <algorithm>
#include <complex>
#include <iostream>
#include <numbers>

namespace mi
{

    //#pragma GCC push_options
    //#pragma GCC optimize ("-Ofast")

    auto normalize(uint16_t value /* 12 bit precision */) -> float
    {
        constexpr uint16_t mid_val = 1 << 11;
        float const normal = (static_cast<float>(value) / mid_val) - 1.F;
        return normal;
    }

    template<std::size_t N, std::size_t Stride, typename Iterator, typename OutIterator>
    void raw_fft(Iterator begin, OutIterator out)
    {
        using namespace std::complex_literals;
        static_assert(N > 0);
        if constexpr (N == 1)
        {
            *out = *begin;
        }
        else
        {
            raw_fft<N / 2, Stride * 2>(begin, out);
            raw_fft<N / 2, Stride * 2>(begin + Stride, out + N / 2);

            for (std::size_t k = 0; k < N / 2; ++k)
            {
                float const t = static_cast<float>(k) / N;
                std::complex<float> const f = -2.F * std::numbers::pi_v<float> * t * 1if;
                std::complex<float> const v = std::exp(f) * out[k + N / 2];
                std::complex<float> const e = out[k];
                out[k] = e + v;
                out[k + N / 2] = e - v;
            }
        }
    }

    template<std::size_t Samples, typename Iterator, typename OutIterator>
    void window(Iterator begin, OutIterator out)
    {
        for (std::size_t i = 0; i < Samples; ++i)
        {
            float const t = static_cast<float>(i) / (Samples - 1);
            float const hann = 0.5 - 0.5 * cosf(2 * std::numbers::pi_v<float> * t);
            *(out + i) = *(begin + i) * hann;
        }
    }

    [[nodiscard]] auto amplitude(std::complex<float> const& value) -> float
    {
        float amp = 8.F;
        float const a = (std::abs(value) + 1.F) * amp;
        return std::log(a * a);
    }

    template<std::size_t Samples, typename Iterator, typename OutIterator>
    [[nodiscard]] auto smooth(Iterator begin, OutIterator out, float dt)
    {
        constexpr float smoothness_factor = 20.F;
        for (std::size_t i = 0; i < Samples / 2; ++i)
        {
            auto& v = *(begin + i + 1);
            auto& o = *(out + i);
            o += (amplitude(v) - o) * smoothness_factor * dt;
        }
    }

    template<std::size_t Samples, typename Iterator, typename OutIterator>
    void quantize(Iterator begin, OutIterator out)
    {
        for (std::size_t i = 0; i < Samples / 2; ++i)
        {
            auto v = *(begin + i);
            *(out + i) = static_cast<uint8_t>(UINT8_MAX * v);
        }
    }

    template<std::size_t Samples>
    auto fft(std::span<const uint16_t, Samples> input, SetFrequencyData freq_data, float dt)
        -> tl::expected<std::span<uint8_t>, Error>
    {
        static std::array<float, Samples> normalized;
        std::ranges::transform(input, normalized.begin(), normalize);

        static std::array<float, Samples> windowed;
        window<Samples>(normalized.begin(), windowed.begin());

        static std::array<std::complex<float>, Samples> raw{};
        const char* err;
        simple_fft::FFT(windowed, raw, Samples, err);

        static std::array<float, Samples / 2> smoothed{};
        smooth<Samples>(raw.begin(), smoothed.begin(), dt);

        static std::array<uint8_t, Samples / 2> quantized;
        quantize<Samples>(smoothed.begin(), quantized.begin());

        return std::span<uint8_t>{quantized.begin(), Samples / 2};
    }

    //#pragma GCC pop_options
} // namespace mi
