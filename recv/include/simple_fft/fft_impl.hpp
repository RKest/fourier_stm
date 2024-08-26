/**
 * Copyright (c) 2013-2020 Dmitry Ivanov
 *
 * This file is a part of Simple-FFT project and is distributed under the terms
 * of MIT license: https://opensource.org/licenses/MIT
 */

#ifndef __SIMPLE_FFT__FFT_IMPL_HPP__
#define __SIMPLE_FFT__FFT_IMPL_HPP__

#include "fft_settings.h"
#include "error_handling.hpp"
#include <cstddef>
#include <cmath>
#include <vector>

using std::size_t;

#ifndef M_PI
#define M_PI 3.1415926535897932
#endif

namespace simple_fft {
namespace impl {

enum FFT_direction
{
    FFT_FORWARD = 0,
    FFT_BACKWARD
};

// checking whether the size of array dimension is power of 2
// via "complement and compare" method
inline bool isPowerOfTwo(const size_t num)
{
    return num && (!(num & (num - 1)));
}

inline bool checkNumElements(const size_t num_elements, const char *& error_description)
{
    using namespace error_handling;

    if (!isPowerOfTwo(num_elements)) {
        GetErrorDescription(EC_ONE_OF_DIMS_ISNT_POWER_OF_TWO, error_description);
        return false;
    }

    return true;
}

template <class TComplexArray1D>
inline void scaleValues(TComplexArray1D & data, const size_t num_elements)
{
    real_type mult = 1.0 / num_elements;
    int num_elements_signed = static_cast<int>(num_elements);

#ifndef __clang__
#ifdef __USE_OPENMP
#pragma omp parallel for
#endif
#endif
    for(int i = 0; i < num_elements_signed; ++i) {
#ifdef __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
        data[i] *= mult;
#else
        data(i) *= mult;
#endif
    }
}

// NOTE: explicit template specialization for the case of std::vector<complex_type>
// because it is used in 2D and 3D FFT for both array classes with square and round
// brackets of element access operator; I need to guarantee that sub-FFT 1D will
// use square brackets for element access operator anyway. It is pretty ugly
// to duplicate the code but I haven't found more elegant solution.
template <>
inline void scaleValues<std::vector<complex_type> >(std::vector<complex_type> & data,
                                                    const size_t num_elements)
{
    real_type mult = 1.0 / num_elements;
    int num_elements_signed = static_cast<int>(num_elements);

#ifndef __clang__
#ifdef __USE_OPENMP
#pragma omp parallel for
#endif
#endif
    for(int i = 0; i < num_elements_signed; ++i) {
        data[i] *= mult;
    }
}

template <class TComplexArray1D>
inline void bufferExchangeHelper(TComplexArray1D & data, const size_t index_from,
                                 const size_t index_to, complex_type & buf)
{
#ifdef __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
    buf = data[index_from];
    data[index_from] = data[index_to];
    data[index_to]= buf;
#else
    buf = data(index_from);
    data(index_from) = data(index_to);
    data(index_to)= buf;
#endif
}

// NOTE: explicit template specialization for the case of std::vector<complex_type>
// because it is used in 2D and 3D FFT for both array classes with square and round
// brackets of element access operator; I need to guarantee that sub-FFT 1D will
// use square brackets for element access operator anyway. It is pretty ugly
// to duplicate the code but I haven't found more elegant solution.
template <>
inline void bufferExchangeHelper<std::vector<complex_type> >(std::vector<complex_type> & data,
                                                             const size_t index_from,
                                                             const size_t index_to,
                                                             complex_type & buf)
{
    buf = data[index_from];
    data[index_from] = data[index_to];
    data[index_to]= buf;
}

template <class TComplexArray1D>
void rearrangeData(TComplexArray1D & data, const size_t num_elements)
{
    complex_type buf;

    size_t target_index = 0;
    size_t bit_mask;

    for (size_t i = 0; i < num_elements; ++i)
    {
        if (target_index > i)
        {
            bufferExchangeHelper(data, target_index, i, buf);
        }

        // Initialize the bit mask
        bit_mask = num_elements;

        // While bit is 1
        while (target_index & (bit_mask >>= 1)) // bit_mask = bit_mask >> 1
        {
            // Drop bit:
            // & is bitwise AND,
            // ~ is bitwise NOT
            target_index &= ~bit_mask; // target_index = target_index & (~bit_mask)
        }

        // | is bitwise OR
        target_index |= bit_mask; // target_index = target_index | bit_mask
    }
}

template <class TComplexArray1D>
inline void fftTransformHelper(TComplexArray1D & data, const size_t match,
                               const size_t k, complex_type & product,
                               const complex_type factor)
{
#ifdef __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
    product = data[match] * factor;
    data[match] = data[k] - product;
    data[k] += product;
#else
    product = data(match) * factor;
    data(match) = data(k) - product;
    data(k) += product;
#endif
}

// NOTE: explicit template specialization for the case of std::vector<complex_type>
// because it is used in 2D and 3D FFT for both array classes with square and round
// brackets of element access operator; I need to guarantee that sub-FFT 1D will
// use square brackets for element access operator anyway. It is pretty ugly
// to duplicate the code but I haven't found more elegant solution.
template <>
inline void fftTransformHelper<std::vector<complex_type> >(std::vector<complex_type> & data,
                                                           const size_t match,
                                                           const size_t k,
                                                           complex_type & product,
                                                           const complex_type factor)
{
    product = data[match] * factor;
    data[match] = data[k] - product;
    data[k] += product;
}

template <class TComplexArray1D>
bool makeTransform(TComplexArray1D & data, const size_t num_elements,
                   const FFT_direction fft_direction, const char *& error_description)
{
    using namespace error_handling;
    using std::sin;

    double local_pi;
    switch(fft_direction)
    {
    case(FFT_FORWARD):
        local_pi = -M_PI;
        break;
    case(FFT_BACKWARD):
        local_pi = M_PI;
        break;
    default:
        GetErrorDescription(EC_WRONG_FFT_DIRECTION, error_description);
        return false;
    }

    // declare variables to cycle the bits of initial signal
    size_t next, match;
    real_type sine;
    real_type delta;
    complex_type mult, factor, product;

    // NOTE: user's complex type should have constructor like
    // "complex(real, imag)", where each of real and imag has
    // real type.

    // cycle for all bit positions of initial signal
    for (size_t i = 1; i < num_elements; i <<= 1)
    {
        next = i << 1;  // getting the next bit
        delta = local_pi / i;    // angle increasing
        sine = sin(0.5 * delta);    // supplementary sin
        // multiplier for trigonometric recurrence
        mult = complex_type(-2.0 * sine * sine, sin(delta));
        factor = 1.0;   // start transform factor

        for (size_t j = 0; j < i; ++j) // iterations through groups
                                       // with different transform factors
        {
            for (size_t k = j; k < num_elements; k += next) // iterations through
                                                            // pairs within group
            {
                match = k + i;
                fftTransformHelper(data, match, k, product, factor);
            }
            factor = mult * factor + factor;
        }
    }

    return true;
}

// Generic template for complex FFT followed by its explicit specializations
template <class TComplexArray, int NumDims>
struct CFFT
{};

// 1D FFT:
template <class TComplexArray1D>
struct CFFT<TComplexArray1D,1>
{
    // NOTE: passing by pointer is needed to avoid using element access operator
    static bool FFT_inplace(TComplexArray1D & data, const size_t size,
                            const FFT_direction fft_direction,
                            const char *& error_description)
    {
        if(!checkNumElements(size, error_description)) {
            return false;
        }

        rearrangeData(data, size);

        if(!makeTransform(data, size, fft_direction, error_description)) {
            return false;
        }

        if (FFT_BACKWARD == fft_direction) {
            scaleValues(data, size);
        }

        return true;
    }
};
} // namespace impl
} // namespace simple_fft

#endif // __SIMPLE_FFT__FFT_IMPL_HPP__
