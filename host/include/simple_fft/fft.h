/**
 * Copyright (c) 2013-2020 Dmitry Ivanov
 *
 * This file is a part of Simple-FFT project and is distributed under the terms
 * of MIT license: https://opensource.org/licenses/MIT
 */

#ifndef __SIMPLE_FFT__FFT_H__
#define __SIMPLE_FFT__FFT_H__

#include <cstddef>

using std::size_t;

/// The public API
namespace simple_fft {

/// FFT and IFFT functions

// in-place, complex, forward
template <class TComplexArray1D>
bool FFT(TComplexArray1D & data, const size_t size, const char *& error_description);

// not-in-place, complex, forward
template <class TComplexArray1D>
bool FFT(const TComplexArray1D & data_in, TComplexArray1D & data_out,
         const size_t size, const char *& error_description);

// not-in-place, real, forward
template <class TRealArray1D, class TComplexArray1D>
bool FFT(const TRealArray1D & data_in, TComplexArray1D & data_out,
         const size_t size, const char *& error_description);

} // namespace simple_fft

#endif // __SIMPLE_FFT__FFT_H__

#include "fft.hpp"
