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

#define DEBUG 0

#ifndef USE_TWO_PHASE
#define USE_TWO_PHASE 1
#endif

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cmath> 

#include <iostream>

#include <pthread.h>

#ifdef __tile__
#include <tmc/cpus.h>
#endif

#include "../misc/get_clock.h"
#include "../misc/misc.h"
#include "../misc/atomic.h"
#include "../misc/assert.h"

const size_t OUTERREPS = 1;
const double CONF95    = 1.96;


double times[OUTERREPS+1], reftime, refsd; 

template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::printPreamble() {
	printf(" Running barrier benchmarks on %zu thread(s)\n", numParticipants);
	printf("   delayLength: %zu\n", delayLength);
}


template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::delay(int delayLength) {
	int   i;
	float a = 0.0; 

	for (i = 0;  i < delayLength;  i++)  a += i; 

	if  (a < 0) printf("%f \n", a);
} 


template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::measureReferenceTime() {
	size_t k;
	uint64_t start, stop;
	double meantime, sd;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("Computing reference time 1\n"); 

	for (k = 0; k <= OUTERREPS; k++) {
		start = get_clock(); 
	
	  spawnLoopReference();  

		stop = get_clock();
    times[k] = (stop - start) * 1.0e6;
	}

	calculateAndPrintStatistics(&meantime, &sd);

	printf("Reference_time_1 =                        %f microseconds +/- %f\n", meantime, CONF95*sd);

	reftime = meantime;
	refsd = sd;  
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
void* _benchmarkReference(void* threadParam) {
	ThreadParam* param = (ThreadParam*)threadParam;
	DYNPAR<BarrierClass, ParticipantType>* dynpar = (DYNPAR<BarrierClass, ParticipantType>*)param->obj;

	// set affinitiy of this thread
#ifdef __tile__
	// printf("Run thread id: %d on tile: %d\n", param->id, param->cpu_id);
	if (tmc_cpus_set_my_cpu(param->cpu_id) != 0) {
		perror("tmc_cpus_set_my_cpu(..) failed\n");
		exit(1);
	}
#endif

	// signal that this thread has registered with the barrier
	param->initialized = true;
	
	for (size_t i = param->id; i < dynpar->numParticipants; i++) {
			DYNPAR<BarrierClass, ParticipantType>::delay(dynpar->delayLength);
	}

	delete param;
	pthread_exit(NULL);
}


template <class BarrierClass, typename ParticipantType>
void* _benchmark(void* threadParam) {
	ThreadParam* param = (ThreadParam*)threadParam;
	DYNPAR<BarrierClass, ParticipantType>* dynpar = (DYNPAR<BarrierClass, ParticipantType>*)param->obj;

	// set affinitiy of this thread
#ifdef __tile__
	// printf("Run thread id: %d on tile: %d\n", param->id, param->cpu_id);
	if (tmc_cpus_set_my_cpu(param->cpu_id) != 0) {
		perror("tmc_cpus_set_my_cpu(..) failed\n");
		exit(1);
	}
#endif

	BarrierClass* const barrier = dynpar->getBarrier();
  //assert(barrier->phase == param->id - 1);
  
	ParticipantType* const participant = new ParticipantType(barrier); //this registers the participant on the barrier
  //assert(barrier->phase == participant->phase);
  
  memory_fence();
	// signal that this thread has registered with the barrier
	param->initialized = true;
		
	if (DEBUG) printf("Thread %zu> succesfully initialized\n", param->id);

	for (size_t i = param->id; i < dynpar->numParticipants; i++) {
		if (DEBUG) printf("Thread %zu> synchronizing on barrier (i:%zu of %zu) ...\n", param->id, i, dynpar->numParticipants);
      


		#if USE_TWO_PHASE
			participant->resume();
			DYNPAR<BarrierClass, ParticipantType>::delay(dynpar->delayLength);
			participant->next();
		#else
			participant->barrier();
			DYNPAR<BarrierClass, ParticipantType>::delay(dynpar->delayLength);
		#endif
		
		if (DEBUG)  printf("Thread %zu>  synchronized on barrier (i:%zu of %zu)\n", param->id, i, dynpar->numParticipants);

//    std::cout.flush();
//    printf("Thread %zu> synchronized on barrier\n", param->id);
	}

	//TODO: if we want to run the benchmark more than once to calc std
	//      this would be the place to remove ourselves as a participant
	//      OR we can just create a new barrier each time ...
	participant->drop();
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
void DYNPAR<BarrierClass, ParticipantType>::spawnLoopReference() {

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
				_benchmarkReference<BarrierClass, ParticipantType>,
				(void*)param);

		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
	  }

		while (!param->initialized) { pthread_yield(); } 
	}
}


template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::spawnLoop(BarrierClass* barrier, ParticipantType* const participant) {

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

		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
		
		// make sure the spawned thread registerd on the barrier
		while (!param->initialized) { pthread_yield(); }
		
    #if DEBUG
			printf("Thread M> thread created and initialize succeeded id: %d\n", i);
			std::cout << "Thread M> synchronizing on barrier ..." << std::endl;
			std::cout.flush();
		#endif
		
			//Main thread barrier sync
		#if USE_TWO_PHASE
			participant->resume();
			DYNPAR<BarrierClass, ParticipantType>::delay(delayLength);
			participant->next();
		#else
			participant->barrier();
			DYNPAR<BarrierClass, ParticipantType>::delay(delayLength);
		#endif
    

		#if DEBUG
      printf("Thread M> synchronized on barrier for i:%d\n", i);
    #endif
	}

	//TODO: I commented this line since it does not seem relevant here
	//barrier->finalize_initialization();
}

template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::measureBarrierPerformance() {
	size_t k; 
	uint64_t start, stop; 
	double meantime, sd;

	printf("\n");
	printf("--------------------------------------------------------\n");
	printf("Computing BARRIER time\n");



	for (k = 0; k < OUTERREPS; k++){
    #if DEBUG
			printf("\n--------------------------------------------------------\n");
			printf("iteration %zu of %zu started\n", k, OUTERREPS);
		#endif
		start  = get_clock(); 
	
		//stverhae: I put this participant addition inside the timed section
		//          It was outside before
		ParticipantType* const participant = new ParticipantType(barrier);

		spawnLoop(barrier, participant);

		stop = get_clock();
		
		times[k] = (stop - start) /** 1.0e6*/;
		
		//join the threads before we star creating new ones in the next outer loop
		for (size_t i = 1; i < numParticipants; i++) {
			pthread_join(threads[i], NULL);
		}

		//TODO
		participant->drop();
	}


	calculateAndPrintStatistics(&meantime, &sd);

	printf("BARRIER time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);

	printf("BARRIER overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}

template <class BarrierClass, typename ParticipantType>
void DYNPAR<BarrierClass, ParticipantType>::calculateAndPrintStatistics(double *mtp, double *sdp) {

	double meantime, totaltime, sumsq, mintime, maxtime, sd, cutoff; 

	size_t i, nr; 

	mintime = 1.0e10;
	maxtime = 0.;
	totaltime = 0.;

	for (i = 0; i < OUTERREPS; i++){
		mintime = (mintime < times[i]) ? mintime : times[i];
		maxtime = (maxtime > times[i]) ? maxtime : times[i];
		totaltime +=times[i]; 
	} 

	meantime  = totaltime / OUTERREPS;
	sumsq = 0; 

	for (i = 0; i < OUTERREPS; i++){
		sumsq += (times[i]-meantime)* (times[i]-meantime); 
	} 
	sd = sqrt(sumsq/(OUTERREPS-1));

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


