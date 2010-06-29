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

#ifndef USE_TWO_PHASE
  #define USE_TWO_PHASE 1
#endif

#define DEBUG 0

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cmath> 

#include <pthread.h>

#ifdef __tile__
  #include <tmc/cpus.h>
#endif

#include "../misc/get_clock.h"
#include "../misc/atomic.h"
#include "../misc/misc.h"


const size_t OUTERREPS = 1;
const double CONF95    = 1.96;


double times[OUTERREPS+1], reftime, refsd; 

template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::printPreamble() {
  printf(" Running barrier benchmarks on %zu thread(s)\n", numParticipants);
  printf("   outerDelayLength: %zu\n", outerDelay);
  printf("   innerReps: %zu\n",   innerReps);
}


template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::delay(int delayLength) {
  int   i;
  float a = 0.0; 
  
  for (i = 0;  i < delayLength;  i++)  a += i; 
  
  if  (a < 0) printf("%f \n", a);
} 


template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::measureReferenceTime() {
  size_t j, k;
  uint64_t start, stop;
  double meantime, sd;
  
  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing reference time 1\n"); 
  
  for (k = 0; k <= OUTERREPS; k++) {
    start = get_clock(); 
		//this probably gets optimized away and screws our reference time
		//not so bad since it should be very small ...
    for (j = 0; j < innerReps; j++) {
    }
    stop = get_clock();
  }
  
  calculateAndPrintStatistics(&meantime, &sd);
  
  printf("Reference_time_1 =                        %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  reftime = meantime;
  refsd = sd;  
}


template <class BarrierClass, typename ParticipantType>
void innerLoop(ABSOH<BarrierClass, ParticipantType>* absoh, BarrierClass* barrier, ParticipantType* const participant) {
  for (size_t j = 0; j < absoh->innerReps; j++) {
		if(DEBUG) printf("thread> Starting innerloop %zu\n", j);
#if USE_TWO_PHASE
    participant->resume();
    participant->next();
#else
    participant->barrier();
#endif
  }
}


class ThreadParam {
public:
  ThreadParam() : obj(NULL), id(0), initialized(false) {}
           void*  obj;
           size_t id;
  volatile bool   initialized;
  
#ifdef __tile__
           int    cpu_id;
#endif
};


template <class BarrierClass, typename ParticipantType>
void* _benchmark(void* threadParam) {
  ThreadParam* param = (ThreadParam*)threadParam;
  ABSOH<BarrierClass, ParticipantType>* absoh = (ABSOH<BarrierClass, ParticipantType>*)param->obj;
  
  // set affinitiy of this thread
#ifdef __tile__
  // printf("Run thread id: %d on tile: %d\n", param->id, param->cpu_id);
  if (tmc_cpus_set_my_cpu(param->cpu_id) != 0) {
    perror("tmc_cpus_set_my_cpu(..) failed\n");
    exit(1);
  }
#endif
  
  BarrierClass* const barrier = absoh->getBarrier();
  
  ParticipantType* const participant = new ParticipantType(barrier); //this registers the participant on the barrier
  
  memory_fence();  // make sure everything is done
  
  // signal that this thread has registered with the barrier
  param->initialized = true;
  
  // synchronize all participants before the actual benchmark
  while (!absoh->initalization_finished) { pthread_yield(); }
  
  size_t k;
  for (k = 0; k <= OUTERREPS; k++){
		if(DEBUG) printf("thread> Starting outerloop %zu\n", k);
    innerLoop(absoh, barrier, participant);
		if(DEBUG) printf("thread> Ended outerloop %zu\n", k);
    
    ABSOH<BarrierClass, ParticipantType>::delay(absoh->outerDelay);
  }
  
  delete param;
  pthread_exit(NULL);
}

#ifdef __tile__
int _find_next_cpu(cpu_set_t* cpus, int& next_cpu) {
  next_cpu++;
  while (next_cpu < TMC_CPUS_MAX_COUNT && !tmc_cpus_has_cpu(cpus, next_cpu)) {
    next_cpu++;
  }
  return next_cpu;
}
#endif

template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::spawnThreads() {
  pthread_t threads[numParticipants];   // thread handles

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
  
  for (size_t i = 1; i < numParticipants; i++) {
    ThreadParam* param = new ThreadParam();
    param->obj = (void*)this;
    param->id  = i;
#ifdef __tile__
    // find a cpu and give it to the new thread
    param->cpu_id = _find_next_cpu(&cpus, next_cpu);
#endif
    
    int rc = pthread_create(&threads[i], NULL, 
                            _benchmark<BarrierClass, ParticipantType>,
                            (void*)param);
    
    // make sure the spawned thread registerd on the barrier
    while (!param->initialized) { pthread_yield(); }
    
    if (rc){
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }
  
  barrier->finalize_initialization();
  memory_fence();
  initalization_finished = true;
}

template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::measureBarrierPerformance() {
  size_t k; 
  uint64_t start, stop; 
  double meantime, sd;
  
  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing BARRIER time\n");
  
  
  ParticipantType* const participant = new ParticipantType(barrier);

  
  spawnThreads();

  for (k = 0; k <= OUTERREPS; k++){
    start  = get_clock(); 
		if(DEBUG) printf("main> Starting outerloop %zu\n", k);
    
    innerLoop(this, barrier, participant);

		if(DEBUG) printf("main> Stopping outerloop %zu\n", k);
   
		stop = get_clock();
	  //costPerBarrier = TotalBarrierExecutionTime / numBarriers	
    times[k] = (stop - start) * 1.0e6 / (double) innerReps;
  
		delay(outerDelay);
  }
  
  
  calculateAndPrintStatistics(&meantime, &sd);
  
  printf("CostPerBarrier =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  printf("BARRIER overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));
  
}

template <class BarrierClass, typename ParticipantType>
void ABSOH<BarrierClass, ParticipantType>::calculateAndPrintStatistics(double *mtp, double *sdp) {
  
  double meantime, totaltime, sumsq, mintime, maxtime, sd, cutoff; 
  
  size_t i, nr; 
  
  mintime = 1.0e10;
  maxtime = 0.;
  totaltime = 0.;
  
  for (i = 1; i <= OUTERREPS; i++){
    mintime = (mintime < times[i]) ? mintime : times[i];
    maxtime = (maxtime > times[i]) ? maxtime : times[i];
    totaltime +=times[i]; 
  } 
  
  meantime  = totaltime / OUTERREPS;
  sumsq = 0; 
  
  for (i = 1; i <= OUTERREPS; i++){
    sumsq += (times[i]-meantime)* (times[i]-meantime); 
  }

#if OUTERREPS > 1
    sd = sqrt(sumsq/(OUTERREPS-1));
#else
    sd = 0;
#endif

  
  cutoff = 3.0 * sd; 
  
  nr = 0; 
  
  for (i=1; i <= OUTERREPS; i++){
    if ( fabs(times[i]-meantime) > cutoff ) nr ++; 
  }
  
  printf("\n"); 
  printf("Sample_size       Average     Min         Max          S.D.          Outliers\n");
  printf(" %zu                %f   %f   %f    %f      %zu\n",OUTERREPS, meantime, mintime, maxtime, sd, nr); 
  printf("\n");
  
  *mtp = meantime; 
  *sdp = sd; 
  
} 


