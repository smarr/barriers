/*
 * Copyright (c) 2010 Stijn Verhaegen, Stefan Marr, Vrije Universiteit Brussel
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

#include <cstdlib>
#include <pthread.h>
#include "../misc/misc.h"

#ifdef __tile__
  #include <tmc/cpus.h>
#endif

typedef void *(*pthread_routine)(void*);

void* _benchmark(void* threadParam);

class SplashStarter;

class ThreadParam {
public:
  ThreadParam(SplashStarter* const starter, size_t id)
  : starter(starter), id(id), initialized(false) {
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
  }
  
  SplashStarter*  const starter;
  const size_t id;
  void     (*func)(ThreadParam*);
#ifdef __tile__
  int    cpu_id;
#endif
  
  void ensure_child_initialization() {
    pthread_mutex_lock(&lock);
    if (!initialized) {
      pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);
  }
  
  void signalAndAwaitInitialization();
  
private:
  pthread_mutex_t lock;
  pthread_cond_t  cond;
  volatile bool initialized;
};

#ifdef __tile__
int _find_next_cpu(cpu_set_t* cpus, int& next_cpu) {
  next_cpu++;
  while (next_cpu < TMC_CPUS_MAX_COUNT && !tmc_cpus_has_cpu(cpus, next_cpu)) {
    next_cpu++;
  }
  return next_cpu;
}
#endif


class SplashStarter {
public:
  
  SplashStarter(const int numParticipants)
  : numParticipants(numParticipants),
    init_completed(false) {
    pthread_mutex_init(&global_init_mtx, NULL);
    pthread_cond_init(&global_init_sig, NULL);  
  }
  
  const size_t numParticipants;
      
  void await_initalization_to_finish() {
    pthread_mutex_lock(&global_init_mtx);
    
      if (!init_completed) {
        pthread_cond_wait(&global_init_sig, &global_init_mtx);
      }
    
    pthread_mutex_unlock(&global_init_mtx);
  }
  
  void spawnThreads(void (*func)(ThreadParam*), pthread_t* threads) {
    // on the tilera we can set the thread affinity, but we need some infos for that
#ifdef __tile__
    cpu_set_t cpus;
    if (tmc_cpus_get_online_cpus(&cpus)) {
      perror("tmc_cpus_get_online_cpus failed\n");
      exit(1);
    }
    
    int next_cpu = -1;  // to make sure the first found cpu has id 0
    _find_next_cpu(&cpus, next_cpu);  //should be 0 or something else if that tile is not available to linux
    
    // printf("Run main thread on tile id: %d\n", next_cpu);
    // now set the current thread to the right CPU
    if (tmc_cpus_set_my_cpu(next_cpu) != 0) {
      perror("tmc_cpus_set_my_cpu(..) failed\n");
      exit(1);
    }
#endif
    
    for (size_t i = 0; i < numParticipants; i++) {
      ThreadParam* param = new ThreadParam(this, i);
      param->func = func;

#ifdef __tile__
      // find a cpu and give it to the new thread
      param->cpu_id = _find_next_cpu(&cpus, next_cpu);
#endif
      
      const int rc = pthread_create(&threads[i], NULL, 
                              _benchmark,
                              (void*)param);

      if (rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
      }
      
      // make sure the spawned thread registerd on the barrier
      param->ensure_child_initialization();
    }
  }
  
  void signal_initalization_finished() {
    pthread_mutex_lock(&global_init_mtx);
    pthread_cond_broadcast(&global_init_sig);
    pthread_mutex_unlock(&global_init_mtx);
  }

private:
  
  volatile bool init_completed;
  
  pthread_mutex_t global_init_mtx;
  pthread_cond_t  global_init_sig;

};

void ThreadParam::signalAndAwaitInitialization() {
  // signal that this thread has registered with all barriers
  pthread_mutex_lock(&lock);
  initialized = true;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);
  
  // synchronize all participants before the actual benchmark
  starter->await_initalization_to_finish();
}


void* _benchmark(void* threadParam) {
  ThreadParam* param = (ThreadParam*)threadParam;
  
  // set affinitiy of this thread
#ifdef __tile__
  // printf("Run thread id: %d on tile: %d\n", param->id, param->cpu_id);
  if (tmc_cpus_set_my_cpu(param->cpu_id) != 0) {
    perror("tmc_cpus_set_my_cpu(..) failed\n");
    exit(1);
  }
#endif
  
  // execute
  (param->func)(param);
  
  delete param;
  pthread_exit(NULL);
}
