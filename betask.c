/* based_mt_task.c -- A basic multi-threaded real-time task skeleton. 
 *
 * This (by itself useless) task demos how to setup a multi-threaded LITMUS^RT
 * real-time task. Familiarity with the single threaded example (base_task.c)
 * is assumed.
 *
 * Currently, liblitmus still lacks automated support for real-time
 * tasks, but internaly it is thread-safe, and thus can be used together
 * with pthreads.
 */
 //clsuter 1 contains cpu 2 and 3
//o-IdeaPad-P500:~/liblitmus$ sudo ./rtspin -p1 -z2 30 100 5  means run in cluster 1 terminal 1
 //sudo ./rtspin -p1 -z2 80 100 5 cluster 1 terminal 2

//cluster 0 contains cpu 2 and 3
//gbaduz@gbaduz-Lenovo-IdeaPad-P500:~/liblitmus$ sudo ./rtspin -p0 -z2 30 100 5 on terminal means cluster size = 2 terminal 2 cluster 0

//gbaduz@gbaduz-Lenovo-IdeaPad-P500:~/liblitmus$ sudo ./rtspin -p0 -z2 80 100 5 on another terminal terminal 2 cluster 0

//for pedf base_mt_task thread 1= 0,1 thread 2= 1,1  
//for pedf base_task thread 1= 2,1 thread 2= 3,1


//for cedf base_mt_task thread 1= 0,2 thread 2= 0,2  
//for cedf base_task thread 1= 1,2 thread 2= 1,2



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>  /* Semaphore */
/* Include gettid() */
#include <sys/types.h>

/* Include threading support. */
#include <pthread.h>

/* Include the LITMUS^RT API.*/
#include "litmus.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <sched.h>
#include <SDL.h>
#include <SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#define DISPLAY_FIFO_PRIO sched_get_priority_max(SCHED_FIFO)
#define DECODE_FIFO_PRIO  (DISPLAY_FIFO_PRIO - 90)


#define PERIOD           40
#define RELATIVE_DEADLINE 40
#define EXEC_COST         10
#define NUMFRAMES 200
/* Let's create 10 threads in the example, 
 * for a total utilization of 1.
 */
#define NUM_THREADS      36 
#define NANO_SECOND_MULTIPLIER  1000000  // 1 millisecond = 1,000,000 Nanoseconds
sem_t mutexemp;
sem_t mutexfull;

char logfile[255]={0x0};
int setschedfunc ();
/* The information passed to each thread. Could be anything. */
struct thread_context {
	int id;
};

/* The real-time thread program. Doesn't have to be the same for
 * all threads. Here, we only have one that will invoke job().
 */

void* rt_threadfetch(void *tcontext);

/* Declare the periodically invoked job. 
 * Returns 1 -> task should exit.
 *         0 -> task should continue.
 */

int jobbusyloop(void);


/* Catch errors.
 */
#define CALL( exp ) do { \
		int ret; \
		ret = exp; \
		if (ret != 0) \
			fprintf(stderr, "%s failed: %m\n", #exp);\
		else \
			fprintf(stderr, "%s ok.\n", #exp); \
	} while (0)


/* Basic setup is the same as in the single-threaded example. However, 
 * we do some thread initiliazation first before invoking the job.
 */

static int settings =2; // 1 for schedfifo 2 for cfs
static struct timespec sleepValue = {0,0};
const long INTERVAL_MS = PERIOD * NANO_SECOND_MULTIPLIER;
int main(int argc, char** argv)
{
	int i;
	struct thread_context ctx[NUM_THREADS];
	pthread_t             task[NUM_THREADS];
	int numofBEJobs;
	FILE *fp;
	/* The task is in background mode upon startup. */		

	if(settings == 1){
		if(setschedfunc()==0)
		exit(1);
	}
	/*****
	 * 1) Command line paramter parsing would be done here.
	 */
	if(argc < 2) {
    fprintf(stderr, "Usage: enter filename\n");
    exit(1);
  }	
	
	strcpy(logfile,"/home/gbaduz/ramfs/");
	strcat(logfile,argv[1]);
	numofBEJobs = atoi(argv[2]);
        if(numofBEJobs > 36){
		fprintf(stderr, "Usage: enter number below 36\n");
    exit(1);
	}
	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */



	/*****
	 * 3) Initialize LITMUS^RT.
	 *    Task parameters will be specified per thread.
	 */
	if(settings == 0){
	init_litmus();
	}


	/***** 
	 * 4) Launch threads.
	 */
	i=0;
	fp=fopen(logfile, "a+");
	fprintf(fp,"#start	end	exectime\n");
	fclose(fp);
	for (i = 0; i < numofBEJobs; i++){
		ctx[i].id = i;		
		pthread_create(task + i, NULL, rt_threadfetch, (void *) (ctx + i));
	}
	
	/*****
	 * 5) Wait for RT threads to terminate.
	 */
	for (i = 0; i < numofBEJobs; i++)
		pthread_join(task[i], NULL);
	


	return 0;
}
int setschedfunc ()
{
	int err;
	const struct sched_param param = {
		.sched_priority = DECODE_FIFO_PRIO,
	};

	err = sched_setscheduler(0, SCHED_FIFO, &param);
			if (err){
				printf("Could not set display to SCHED_FIFO");
				return 0;
			}
return 1;
	
}

void* rt_threadfetch(void *tcontext)
{
	int do_exit;
	struct thread_context *ctx = (struct thread_context *) tcontext;
	struct rt_task param;
	if(settings == 0){
	/* Set up task parameters */
	init_rt_task_param(&param);
	param.exec_cost = ms2ns(EXEC_COST);
	param.period = ms2ns(PERIOD);
	param.relative_deadline = ms2ns(RELATIVE_DEADLINE);

	/* What to do in the case of budget overruns? */
	param.budget_policy = NO_ENFORCEMENT;

	/* The task class parameter is ignored by most plugins. */
	param.cls = RT_CLASS_SOFT;

	/* The priority parameter is only used by fixed-priority plugins. */
	param.priority = LITMUS_LOWEST_PRIORITY;
	//be_migrate_to_cluster(0, 1);
	//param.cpu = cluster_to_first_cpu(0, 1);

	/* Make presence visible. */
	printf("RT Thread %d active.\n", ctx->id);

	/*****
	 * 1) Initialize real-time settings.
	 */
	CALL( init_rt_thread() );

	/* To specify a partition, do
	 *
	 * param.cpu = CPU;
	 * be_migrate_to(CPU);
	 *
	 * where CPU ranges from 0 to "Number of CPUs" - 1 before calling
	 * set_rt_task_param().
	 */
	CALL( set_rt_task_param(gettid(), &param) );

	/*****
	 * 2) Transition to real-time mode.
	 */
	CALL( task_mode(LITMUS_RT_TASK) );

	/* The task is now executing as a real-time task if the call didn't fail. 
	 */
	}


	/*****
	 * 3) Invoke real-time jobs.
	 */
	do {
		/* Wait until the next job is released. */
		
		if(settings == 0){
			
			sleep_next_period();
	
		}
		else{	
			sleepValue.tv_nsec = INTERVAL_MS;
			nanosleep(&sleepValue, NULL);
			
		}
		/* Invoke job. */
		do_exit = jobbusyloop();	
	
	} while (!do_exit);

	
	/*****
	 * 4) Transition to background mode.
	 */
	if(settings == 0){	
	CALL( task_mode(BACKGROUND_TASK) );}


	return NULL;
}
int jobbusyloop(void) 
{	
	struct timespec tstart={0,0}, tend={0,0};
	double timediff;
	FILE *fp; 
	clock_gettime(CLOCK_MONOTONIC, &tstart); 
	

	
	while(1){
		
		clock_gettime(CLOCK_MONOTONIC, &tend);	
		timediff = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec)- ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
		if(timediff*1000 > 2.0){
			//printf("running");
			break;		
		}	
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);	
	timediff = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec)- ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
	
  	fp=fopen(logfile, "a+");
	fprintf(fp,"%.9f	%.9f	%.9f\n",(double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec,(double)tend.tv_sec + 1.0e-9*tend.tv_nsec,(double)timediff);
	fclose(fp);
  
	return 0;

  	
	
}

