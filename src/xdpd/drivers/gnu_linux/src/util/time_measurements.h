/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME_PROFILING_H
#define TIME_PROFILING_H 

#include <assert.h>
#include <rofl/datapath/pipeline/common/datapacket.h>
#include <string.h>
#include <rofl_datapath.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../config.h"
#include "likely.h"

#include <utils/c_logger.h>

/**
* @file time_profiling.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Defines a mechanism to measure the time spent in each of the
* packet processing stages. Useful during profiling 
*/

/*
* Defines the different measuring points within the driver
*/
typedef enum{
	//---------- I/O thread -----------
	TM_S0,		//Get buffer
	TM_S1,		//Buffer inited (copied)
	TM_S2,		//Packet classified
	TM_S3,		//Pre switch process
	TM_S4,		//Pipeline pkt_matches inited
	//Fast path
	TM_SA5_PRE,	//Pre-enqueue in output queue of the port
	TM_SA5_SUCCESS,	//Post enqueue in output queue of the port
	TM_SA5_FAILURE,	//Post enqueue in output queue of the port(failed->dropped) #END
	//Slow path
	TM_SB5_PRE,		//Pre-enqueue in PKT_in
	TM_SB5_SUCCESS,	//Post-enqueue in PKT_in #END
	TM_SB5_FAILURE,	//Post-enqueue in PKT_in (failed->dropped) #END
	//---------- I/O thread ----------
	TM_SA6,		//TX dequeue
	TM_SA7,		//Packet copied #END
		
	TM_MAX,		//Must be the last one
}time_stages_t;

/*
* Defines the time measurement structure
*/
typedef struct{
	//Packet counters
	uint64_t total_pkts;
	uint64_t total_SA5_dropped_pkts;
	uint64_t total_SB5_dropped_pkts;
	uint64_t total_output_pkts;
	uint64_t total_pktin_pkts;

	//Accumulated results
	double accumulated[TM_MAX];

	//Current packet time measurements
	uint64_t current[TM_MAX];
	
	bool s2_reached;
	bool s4_reached; //pkt_in
	bool sb5_reached; //pkt_in
	bool pkt_out; 
}time_measurements_t;

/*
* Global variables (acumulated times)
*/
extern time_measurements_t global_measurements;

#ifdef ENABLE_TIME_MEASUREMENTS

	//Extern C
	ROFL_BEGIN_DECLS

	//From http://stackoverflow.com/questions/6814792/why-is-clock-gettime-so-erratic
	inline uint64_t rdtsc() {
		#if defined(__GNUC__)
		#   if defined(__i386__)
		    uint64_t x;
		    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
		    return x;
		#   elif defined(__x86_64__)
		    uint32_t hi, lo;
		    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
		    return ((uint64_t)lo) | ((uint64_t)hi << 32);
		#   else
		#       error Unsupported architecture.
		#   endif
		#elif defined(_MSC_VER)
		    return __rdtsc();
		#else
		#   error Other compilers not supported...
		#endif
	}

	//Initializes
	inline void tm_init_pkt(time_measurements_t* tm){
		memset(tm,0,sizeof(time_measurements_t));
	}

	inline void tm_stamp_stage(time_measurements_t* tm, time_stages_t stage){
		uint64_t now;
		if(unlikely(tm->pkt_out)){
			return;
		}
		
		now = rdtsc();

		switch(stage){
			case TM_S0:
				memset(tm->current,0,sizeof(uint64_t)*TM_MAX);
				tm->s2_reached = false;
				tm->sb5_reached = false;
				tm->pkt_out = false;
				//fprintf(stderr, "************\n");
				break;
			case TM_S1:
				assert(tm->current[TM_S0] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_S0];
				//fprintf(stderr, "S1 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S0]);
				break;
			case TM_S2:
				assert(tm->current[TM_S1] != 0.0);
				if(!tm->s4_reached){ //May be called more than one time
					tm->accumulated[stage] += now - tm->current[TM_S1];
					tm->s2_reached = true;
					//fprintf(stderr, "S2 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S1]);
				}
				break;
			case TM_S3:
				assert(tm->current[TM_S2] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_S2];
				//fprintf(stderr, "S3 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S2]);
				break;
			case TM_S4:
				assert(tm->current[TM_S3] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_S3];		//Avoid race condition
				//fprintf(stderr, "S4 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S3]);
				tm->s4_reached = true;
				break;
			case TM_SA5_PRE:
				assert(tm->current[TM_S4] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_S4];
				//fprintf(stderr, "SA5_PRE now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S4]);
				break;
			case TM_SA5_SUCCESS:
				if(tm->current[TM_SA5_PRE] != 0.0){ //PKT_OUT or flow_mod
					tm->accumulated[stage] += now - tm->current[TM_SA5_PRE];	//Avoid race condition
					//fprintf(stderr, "SA5_SUCCESS now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SA5_PRE]);	
				}
				break;
			case TM_SA5_FAILURE:
				assert(tm->current[TM_SA5_PRE] != 0.0);
				tm->accumulated[stage] = now - tm->current[TM_SA5_PRE];
				tm->total_SA5_dropped_pkts++;
				tm->total_pkts++;
				//fprintf(stderr, "SA5_FAILURE now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SA5_PRE]);	
				break;
			case TM_SB5_PRE:
				assert(tm->current[TM_S4] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_S4];
				tm->sb5_reached = true;
				//fprintf(stderr, "SB5_PRE now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_S4]);	
				break;
			case TM_SB5_SUCCESS:
				assert(tm->current[TM_SB5_PRE] != 0.0);
				tm->accumulated[stage] += now - tm->current[TM_SB5_PRE];
				tm->total_pktin_pkts++;	
				tm->total_pkts++;
				//fprintf(stderr, "SB5_SUCCESS now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SB5_PRE]);	
				break;
			case TM_SB5_FAILURE:
				assert(tm->current[TM_SB5_PRE] != 0.0);
				tm->accumulated[stage] = now - tm->current[TM_SB5_PRE];
				tm->total_SB5_dropped_pkts++;	
				tm->total_pkts++;
				//fprintf(stderr, "SB5_FAILURE now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SB5_PRE]);	
				break;
			case TM_SA6:
				if(tm->s4_reached && !tm->sb5_reached){ //May be called more than one time
					assert(tm->current[TM_SA5_PRE] != 0.0);
					tm->accumulated[stage] += now - tm->current[TM_SA5_PRE];	//Avoid race-condition
					//fprintf(stderr, "SA6 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SA5_PRE]);	
				}
				break;
			case TM_SA7:
				if(tm->s4_reached && !tm->sb5_reached){ //May be called more than one time
					assert(tm->current[TM_SA6] != 0.0);
					tm->accumulated[stage] += now - tm->current[TM_SA6];
					tm->total_output_pkts++;	
					tm->total_pkts++;
					//fprintf(stderr, "SA7 now: %"PRIu64", diff: %"PRIu64"\n", now, now - tm->current[TM_SA6]);	
				}
				break;
			case TM_MAX:
				assert(0);
				break;
		}

		tm->current[stage] = now;
	}

	inline void tm_aggregate_pkt(time_measurements_t* tm){
		unsigned int i;

		//Copy accumulated diffs for stages	
		for(i=0;i<TM_MAX;i++)
			global_measurements.accumulated[i] += tm->accumulated[i];	

		//Copy pkt counters
		global_measurements.total_pkts += tm->total_pkts;
		global_measurements.total_SA5_dropped_pkts += tm->total_SA5_dropped_pkts;
		global_measurements.total_SB5_dropped_pkts += tm->total_SB5_dropped_pkts;
		global_measurements.total_output_pkts += tm->total_output_pkts;
		global_measurements.total_pktin_pkts += tm->total_pktin_pkts;
	}

	void tm_dump_measurements(void);
	void tm_dump_pkt(struct datapacket* pkt)  __attribute__((used));

	//Extern C
	ROFL_END_DECLS

	//Define TM state
	#define TM_PKT_STATE time_measurements_t tm_state;

	//Initialization of the time structure of the datapacket
	//Only done once
	#define TM_INIT_PKT(pkt_p)\
		do{\
			tm_init_pkt(&((datapacketx86*)pkt_p->platform_state)->tm_state);\
		}while(0)

	//Tags stage and saves it the "current". If the stage is a dead-end
	//it accumulates the differential values into the accumlated array
	#define TM_STAMP_STAGE(pkt_p, stage)\
		do{\
			if(pkt_p->is_replica == false)\
			tm_stamp_stage(& ( ( (datapacketx86*) pkt_p->platform_state ) ->tm_state ), stage);\
		}while(0)

	#define TM_STAMP_STAGE_DPX86(pkt_dpx86, stage)\
		do{\
			tm_stamp_stage(&pkt_dpx86->tm_state , stage);\
		}while(0)

	//Stamp as PKT_OUT (ignore)
	#define TM_STAMP_PKT_OUT(pkt_p)\
		do{\
			((datapacketx86*)pkt_p->platform_state)->tm_state.pkt_out = true;\
		}while(0)


	//PKT accumulation to general counters
	#define TM_AGGREGATE_PKT(pkt_p) tm_aggregate_pkt(& ( ( (datapacketx86*) pkt_p->platform_state ) ->tm_state ))	

	//Dump measurements stats
	#define TM_DUMP_MEASUREMENTS() tm_dump_measurements()	

#else //ENABLE_TIME_MEASUREMENTS

	//Define empty macros
	#define TM_PKT_STATE 
	#define TM_INIT_PKT(...) do{}while(0)
	#define TM_STAMP_STAGE(...) do{}while(0)
	#define TM_STAMP_STAGE_DPX86(...) do{}while(0)
	#define TM_STAMP_PKT_OUT(...) do{}while(0)
	#define TM_AGGREGATE_PKT(...) do{}while(0)
	#define TM_DUMP_MEASUREMENTS(...) do{}while(0)


#endif //ENABLE_TIME_MEASUREMENTS
#endif /* TIME_PROFILING_H_ */
