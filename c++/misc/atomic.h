/*
 * Copyright (c) 2010 Stefan Marr, Vrije Universiteit Brussel
 * <http://www.stefan-marr.de/>, <http://code.google.com/p/barriers/>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *  This header file defines common atomic operations, and provides
 *  implementations for GCC and tile-cc
 */

#include "misc.h"

# ifdef __TILECC__
  # include <atomic.h>
  # include <tmc/mem.h>
# endif

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

/**
  Adds an increment to the given memory location, and returns the new value
 */
inline int atomic_add_and_fetch(int* mem, int increment) {
# ifdef __TILECC__
  int oldValue = atomic_add_val(mem, increment);
  return oldValue + increment;
# else
  return __sync_add_and_fetch(mem, increment);
# endif
}

/**
 * @return the old value of *ptr
 */
inline int atomic_compare_and_swap(int* ptr, int oldValue, int newValue) {
# ifdef __TILECC__
  return atomic_compare_and_exchange_val_acq(ptr, newValue, oldValue);
# else
  return __sync_val_compare_and_swap(ptr, oldValue, newValue);
# endif
}

/**
 * @return true if CAS was sucessful
 */
inline bool atomic_compare_and_swap_bool(int* ptr, int oldValue, int newValue) {
# ifdef __TILECC__
  return NULL == atomic_compare_and_exchange_bool_acq(ptr, newValue, oldValue);
# else
  return __sync_bool_compare_and_swap(ptr, oldValue, newValue);
# endif
}


inline void memory_fence() {
# ifdef __TILECC__
  tmc_mem_fence();
# else
  __sync_synchronize();
# endif
}

#endif
