/*--

This file is a part of libsais, a library for linear time
suffix array and burrows wheeler transform construction.

   Copyright (c) 2021-2022 Ilya Grebnov <ilya.grebnov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

Please see the file LICENSE for full copyright information.

--*/

#ifndef LIBSAIS64_H
#define LIBSAIS64_H 1

#ifdef __cplusplus
extern "C" {
#endif

    #include <stdint.h>

    /**
    * Constructs the suffix array of a given string.
    * @param T [0..n-1] The input string.
    * @param SA [0..n-1+fs] The output array of suffixes.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of SA array (0 should be enough for most cases).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    int64_t libsais64(const uint8_t * T, int64_t * SA, int64_t n, int64_t fs, int64_t * freq);



    /**
    * Constructs the burrows-wheeler transformed string of a given string.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (0 should be enough for most cases).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @return The primary index if no error occurred, -1 or -2 otherwise.
    */
    int64_t libsais64_bwt(const uint8_t * T, uint8_t * U, int64_t * A, int64_t n, int64_t fs, int64_t * freq);

    /**
    * Constructs the burrows-wheeler transformed string of a given string with auxiliary indexes.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n-1+fs] The temporary array.
    * @param n The length of the given string.
    * @param fs The extra space available at the end of A array (0 should be enough for most cases).
    * @param freq [0..255] The output symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The output auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    int64_t libsais64_bwt_aux(const uint8_t * T, uint8_t * U, int64_t * A, int64_t n, int64_t fs, int64_t * freq, int64_t r, int64_t * I);



    /**
    * Constructs the original string from a given burrows-wheeler transformed string with primary index.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param i The primary index.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    int64_t libsais64_unbwt(const uint8_t * T, uint8_t * U, int64_t * A, int64_t n, const int64_t * freq, int64_t i);

    /**
    * Constructs the original string from a given burrows-wheeler transformed string with auxiliary indexes.
    * @param T [0..n-1] The input string.
    * @param U [0..n-1] The output string (can be T).
    * @param A [0..n] The temporary array (NOTE, temporary array must be n + 1 size).
    * @param n The length of the given string.
    * @param freq [0..255] The input symbol frequency table (can be NULL).
    * @param r The sampling rate for auxiliary indexes (must be power of 2).
    * @param I [0..(n-1)/r] The input auxiliary indexes.
    * @return 0 if no error occurred, -1 or -2 otherwise.
    */
    int64_t libsais64_unbwt_aux(const uint8_t * T, uint8_t * U, int64_t * A, int64_t n, const int64_t * freq, int64_t r, const int64_t * I);



#ifdef __cplusplus
}
#endif

#endif
