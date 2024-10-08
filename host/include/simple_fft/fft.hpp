/**
 * Copyright (c) 2013-2020 Dmitry Ivanov
 *
 * This file is a part of Simple-FFT project and is distributed under the terms
 * of MIT license: https://opensource.org/licenses/MIT
 */

#ifndef __SIMPLE_FFT__FFT_HPP__
#define __SIMPLE_FFT__FFT_HPP__

#include "copy_array.hpp"
#include "fft_impl.hpp"

namespace simple_fft {

// in-place, complex, forward
template <class TComplexArray1D>
bool FFT(TComplexArray1D & data, const size_t size, const char *& error_description)
{
    return impl::CFFT<TComplexArray1D,1>::FFT_inplace(data, size, impl::FFT_FORWARD,
                                                      error_description);
}

// not-in-place, complex, forward
template <class TComplexArray1D>
bool FFT(const TComplexArray1D & data_in, TComplexArray1D & data_out,
         const size_t size, const char *& error_description)
{
    copy_array::copyArray(data_in, data_out, size);
    return impl::CFFT<TComplexArray1D,1>::FFT_inplace(data_out, size, impl::FFT_FORWARD,
                                                      error_description);
}

// not-in-place, real, forward
template <class TRealArray1D, class TComplexArray1D>
bool FFT(const TRealArray1D & data_in, TComplexArray1D & data_out,
         const size_t size, const char *& error_description)
{
    copy_array::copyArray(data_in, data_out, size);
    return impl::CFFT<TComplexArray1D,1>::FFT_inplace(data_out, size,
                                                      impl::FFT_FORWARD,
                                                      error_description);
}

} // simple_fft

#endif // __SIMPLE_FFT__FFT_HPP__
