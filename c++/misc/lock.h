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
 */

#if defined __MACH__
  #define BARRIER_PTHREAD_MUTEX
#elif defined __TILECC__
  #define BARRIER_TMC_SPIN_MUTEX
#else
  #define BARRIER_PTHREAD_SPINLOCK
#endif

#ifdef BARRIER_PTHREAD_MUTEX
#include <pthread.h>
#define lock_init     pthread_mutex_init
#define lock_destruct pthread_mutex_destroy
#define lock_acquire  pthread_mutex_lock
#define lock_release  pthread_mutex_unlock
#define lock_t        pthread_mutex_t
#endif

#ifdef BARRIER_PTHREAD_SPINLOCK
#include <pthread.h>
#define lock_init     pthread_spin_init
#define lock_destruct pthread_spin_destroy
#define lock_acquire  pthread_spin_lock
#define lock_release  pthread_spin_unlock
#define lock_t        pthread_spinlock_t
#endif

#ifdef BARRIER_TMC_SPIN_MUTEX
#include <tmc/spin.h>
#define lock_init(lock, attr) tmc_spin_mutex_init(lock)
#define lock_destruct(lock) {;} // nothing
#define lock_acquire        tmc_spin_mutex_lock
#define lock_release        tmc_spin_mutex_unlock
#define lock_t              tmc_spin_mutex_t
#endif

