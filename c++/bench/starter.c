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

#ifndef __tile__
  #define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <misc/misc.h>
#include <barriers/barrier.h>

#if defined(__tile__)
  #include <tmc/cpus.h>
#else
//  #include <sys/syscall.h>
//  #include <sys/types.h>
//  #include <sched.h>
#endif

#include "starter.h"

void thread_param_init(thread_param_t* const param, starter_t* const starter, int id) {
  param->starter     = starter;
  param->id          = id;
  param->initialized = false;
  pthread_mutex_init(&param->lock, NULL);
  pthread_cond_init (&param->cond, NULL);
}

void thread_param_ensure_child_initialization(thread_param_t* const tp) {
  pthread_mutex_lock(&tp->lock);
  if (!tp->initialized) {
    pthread_cond_wait(&tp->cond, &tp->lock);
  }
  pthread_mutex_unlock(&tp->lock);
}

void starter_await_initalization_to_finish(starter_t* const starter) {
  pthread_mutex_lock(&starter->global_init_mtx);
  
  if (!starter->init_completed) {
    pthread_cond_wait(&starter->global_init_sig, &starter->global_init_mtx);
  }
  
  pthread_mutex_unlock(&starter->global_init_mtx);
}

void thread_param_signalAndAwaitInitialization(thread_param_t* const tp) {
  // signal that this thread has registered with all barriers
  pthread_mutex_lock(&tp->lock);
  tp->initialized = true;
  pthread_cond_broadcast(&tp->cond);
  pthread_mutex_unlock  (&tp->lock);
  
  // synchronize all participants before the actual benchmark
  starter_await_initalization_to_finish(tp->starter);
}


#ifdef __tile__
int _find_next_cpu(cpu_set_t* cpus, int* const next_cpu) {
  (*next_cpu)++;
  while ((*next_cpu) < TMC_CPUS_MAX_COUNT && !tmc_cpus_has_cpu(cpus, *next_cpu)) {
    (*next_cpu)++;
  }
  
  return (*next_cpu);
}
#endif


void starter_init(starter_t* const starter, const int numParticipants) {
  starter->num_participants = numParticipants;
  starter->init_completed   = false;
  pthread_mutex_init(&starter->global_init_mtx, NULL);
  pthread_cond_init (&starter->global_init_sig, NULL);  
}


void* _benchmark(void* threadParam) {
  thread_param_t* param = (thread_param_t*)threadParam;
  
  // set affinitiy of this thread
#if defined(__tile__)
  // printf("Run thread id: %d on tile: %d\n", param->id, param->cpu_id);
  if (tmc_cpus_set_my_cpu(param->cpu_id) != 0) {
    perror("tmc_cpus_set_my_cpu(..) failed\n");
    exit(1);
  }
#else
  cpu_set_t affinity_mask;
  CPU_ZERO(&affinity_mask);
  CPU_SET(param->cpu_id, &affinity_mask);
  
  if (pthread_setaffinity_np(pthread_self(), sizeof(affinity_mask), &affinity_mask) < 0) {
  //if (sched_setaffinity(syscall(SYS_gettid) /* gettid() */, sizeof(affinity_mask), &affinity_mask) < 0) { //stefan changed getpid to gettid, without testing
    perror("Failed to set affinity");
    abort();
  }
#endif
  
  // execute
  (param->func)(param);
  
  free(param);
  pthread_exit(NULL);
}


void starter_spawn_threads(starter_t* const starter, void (*func)(thread_param_t*), pthread_t* threads) {
  // on the tilera we can set the thread affinity, but we need some infos for that
  int next_cpu = -1;  // to make sure the first found cpu has id 0
#ifdef __tile__
  cpu_set_t cpus;
  if (tmc_cpus_get_online_cpus(&cpus)) {
    perror("tmc_cpus_get_online_cpus failed\n");
    exit(1);
  }
  
  _find_next_cpu(&cpus, &next_cpu);  //should be 0 or something else if that tile is not available to linux
  
  // printf("Run main thread on tile id: %d\n", next_cpu);
  // now set the current thread to the right CPU
  if (tmc_cpus_set_my_cpu(next_cpu) != 0) {
    perror("tmc_cpus_set_my_cpu(..) failed\n");
    exit(1);
  }
#endif
  int i;
  for (i = 0; i < starter->num_participants; i++) {
    thread_param_t* param = (thread_param_t*)malloc(sizeof(thread_param_t));
    thread_param_init(param, starter, i);
    param->func = func;
    
#if defined(__tile__)
    // find a cpu and give it to the new thread
    param->cpu_id = _find_next_cpu(&cpus, &next_cpu);
#else
    next_cpu++;
    param->cpu_id = next_cpu;
#endif
    
    const int rc = pthread_create(&threads[i], NULL, 
                                  _benchmark,
                                  (void*)param);
    
    if (rc){
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
    
    // make sure the spawned thread registerd on the barrier
    thread_param_ensure_child_initialization(param);
  }
}

/**
 * To be called from the managing thread to conclude initialization globally
 */
void starter_signal_initalization_finished(starter_t* const starter) {
  pthread_mutex_lock    (&starter->global_init_mtx);
  pthread_cond_broadcast(&starter->global_init_sig);
  pthread_mutex_unlock  (&starter->global_init_mtx);
}

