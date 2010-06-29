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
 *  This file encodes all necessary helper functionallity to start
 *  threads and make sure they registered on the barriers correctly before
 *  going on in the spawing thread.
 *
 *  This is basically equal to a 'clocked' or 'phased' statement in X10/Habanero. 
 */

#ifndef __STARTER_H__
#define __STARTER_H__

#include <pthread.h>
#include <misc/misc.h>


//#include <stdlib.h>
//#include <stdio.h>
//#include <barriers/barrier.h>

typedef struct starter_t {
  int num_participants;
  volatile bool init_completed;
  pthread_mutex_t global_init_mtx;
  pthread_cond_t  global_init_sig;
} starter_t;

typedef struct thread_param_t {
  starter_t* starter;
  int        id;
  void      (*func)(struct thread_param_t*);
  int        cpu_id;  // only used on tilera at the moment
  pthread_mutex_t lock;
  pthread_cond_t  cond;
  volatile bool initialized;
} thread_param_t;

void thread_param_init(thread_param_t* const param, starter_t* const starter, int id);
void thread_param_ensure_child_initialization(thread_param_t* const tp);
void thread_param_signalAndAwaitInitialization(thread_param_t* const tp);

void starter_init(starter_t* const starter, const int numParticipants);
void starter_await_initalization_to_finish(starter_t* const starter);
void starter_spawn_threads(starter_t* const starter, void (*func)(thread_param_t*), pthread_t* threads);

/**
 * To be called from the managing thread to conclude initialization globally
 */
void starter_signal_initalization_finished(starter_t* const starter);

#endif

