#pragma once

#define __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
#include "message_definitions.hpp"
#include "arm_math.h"
#include <algorithm>
#include <complex>
#include <iostream>
#include <numbers>

namespace mi
{
#pragma GCC push_options
#pragma GCC optimize ("-Ofast")

    auto normalize(uint16_t value /* 12 bit precision */) -> float
    {
        constexpr uint16_t mid_val = 1 << 11;
        float const normal = (static_cast<float>(value) / mid_val) - 1.F;
        return normal;
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

    [[nodiscard]] auto amplitude(float real, float imag) -> float
    {
    	return std::log(std::sqrt(real * real + imag * imag));
    }

    template<std::size_t Samples>
    [[nodiscard]] auto fft_impl(std::span<float, Samples> in) -> std::span<float, Samples / 2>
    {
    	static arm_rfft_fast_instance_f32 rfft_inst;
    	[[maybe_unused]] static arm_status once = []{
    		return arm_rfft_fast_init_f32(&rfft_inst, Samples);
    	}();
    	std::array<float, Samples * 2> out;
    	arm_rfft_fast_f32(&rfft_inst, in.data(), out.data(), 0);
    	std::array<float, Samples / 2> amps;
    	for (std::size_t i = 0; i < amps.size(); ++i)
    	{
    		amps[i] = amplitude(out[2 * i], out[2 * i + 1]);
    	}
    	return std::span<float, Samples / 2>{amps};
    }

    template<std::size_t Samples>
    [[nodiscard]] auto squash(std::span<float, Samples / 2> amps) -> std::span<float>
    {
    	static std::array<float, Samples / 2> store;
    	size_t m = 0;
    	auto push = [&store, &m](float f)
		{
    		store[m++] = f;
		};
        float step = 1.059F;
        float lowf = 1.F;
        float max_amp = 1.0F;
        for (float f = lowf; static_cast<std::size_t>(f) < Samples / 2; f = std::ceil(f*step)) {
            std::size_t f1 = static_cast<std::size_t>(std::ceil(f*step));
            float a = 0.0f;
            for (std::size_t q = static_cast<std::size_t>(f); q < Samples / 2 && q < f1; ++q) {
            	a = std::max(amps[q], a);
            }
            max_amp = std::max(max_amp, a);
            push(a);
        }

        for (std::size_t i = 0; i < m; ++i)
        {
        	store[i] /= max_amp;
        }

        return std::span<float>{store.data(), m};
    }

    template<typename Iterator, typename OutIterator>
    [[nodiscard]] auto smooth(Iterator begin, OutIterator out, float dt, std::size_t m)
    {
        constexpr float smoothness_factor = 8.F;
        for (std::size_t i = 0; i < m; ++i)
        {
            auto& v = *(begin + i);
            auto& o = *(out + i);
            o += (v - o) * smoothness_factor * dt;
        }
    }

    template<typename Iterator, typename OutIterator>
    void quantize(Iterator begin, OutIterator out, std::size_t m)
    {
        for (std::size_t i = 0; i < m; ++i)
        {
            auto v = std::min(*(begin + i), 1.F);
            *(out + i) = static_cast<uint8_t>(UINT8_MAX * v);
        }
    }

    template<std::size_t Samples>
    auto fft(std::span<const uint16_t, Samples> input, float dt)
        -> tl::expected<std::span<uint8_t>, Error>
    {
        static std::array<float, Samples> normalized;
        std::ranges::transform(input, normalized.begin(), normalize);

        static std::array<float, Samples> windowed;
        window<Samples>(normalized.begin(), windowed.begin());

        std::span<float, Samples / 2> raw = fft_impl<Samples>(windowed);

        std::span<float> squashed = squash<Samples>(raw);

        static std::array<float, Samples / 2> smoothed{};
        smooth(squashed.begin(), smoothed.begin(), dt, squashed.size());

        static std::array<uint8_t, Samples / 2> quantized;
        quantize(smoothed.begin(), quantized.begin(), squashed.size());

        return std::span<uint8_t>{quantized.begin(), squashed.size()};
    }

#pragma GCC pop_options
} // namespace mi
