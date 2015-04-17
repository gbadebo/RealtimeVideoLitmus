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


#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>  /* Semaphore */
/* Include gettid() */
#include <sys/types.h>

/* Include threading support. */
#include <pthread.h>
#include <sched.h>
/* Include the LITMUS^RT API.*/
#include "litmus.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>
#include <SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

#define DISPLAY_FIFO_PRIO sched_get_priority_max(SCHED_FIFO)
#define DECODE_FIFO_PRIO  (DISPLAY_FIFO_PRIO - 90)
#define PERIOD           40
#define RELATIVE_DEADLINE 40
#define EXEC_COST         10
#define NUMFRAMES 200
/* Let's create 10 threads in the example, 
 * for a total utilization of 1.
 */
#define NUM_THREADS      3 
#define NANO_SECOND_MULTIPLIER  1000000  // 1 millisecond = 1,000,000 Nanoseconds
sem_t mutexemp;
sem_t mutexfull;

pthread_mutex_t count_mutex;
pthread_cond_t decode_threshold_cv;
pthread_cond_t read_threshold_cv;

pthread_cond_t end_threshold_cv;

/* The information passed to each thread. Could be anything. */
struct thread_context {
	int id;
};

/* The real-time thread program. Doesn't have to be the same for
 * all threads. Here, we only have one that will invoke job().
 */
void* rt_threaddec(void *tcontext);
void* rt_threaddis(void *tcontext);
void* rt_threadfetch(void *tcontext);
/* Declare the periodically invoked job. 
 * Returns 1 -> task should exit.
 *         0 -> task should continue.
 */
int  setschedfunc ();
int jobfetch(void);
int jobdecode(void);

int jobdisplay(void);

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
AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL; 
  AVPacket        packet;
  int             frameFinished;
  //float           aspect_ratio;

  AVDictionary    *optionsDict = NULL;
  struct SwsContext *sws_ctx = NULL;

  SDL_Overlay     *bmp = NULL;
  SDL_Surface     *screen = NULL;
  SDL_Rect        rect;
  SDL_Event       event;
	AVPicture pict;
char filename[255] = {0x0};
char logfile[255]={0x0};
struct timespec tinitial={0,0};
struct timespec tstart={0,0};
struct timespec tfinal={0,0};
int videocode() {
  
	 FILE  *fp;
  //if(argc < 2) {
  //  fprintf(stderr, "Usage: test <file>\n");
  //  exit(1);
 // }
  // Register all formats and codecs
  av_register_all();
  
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  // Open video file
  if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0)
    return -1; // Couldn't open file
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information
  
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, filename, 0);
  
  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  if(videoStream==-1)
    return -1; // Didn't find a video stream
  
  // Get a pointer to the codec context for the video stream
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;
  
  // Find the decoder for the video stream
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
  
  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
    return -1; // Could not open codec
  
  // Allocate video frame
  pFrame=avcodec_alloc_frame();

  // Make a screen to put our video
#ifndef __DARWIN__
        screen = SDL_SetVideoMode(640, 480, 0, 0);
#else
        screen = SDL_SetVideoMode(640, 480, 24, 0);
#endif
  if(!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }
  
// Allocate a place to put our YUV image on that screen
  bmp = SDL_CreateYUVOverlay(pCodecCtx->width,
				 pCodecCtx->height,
				 SDL_YV12_OVERLAY,
				 screen);

sws_ctx =
    sws_getContext
    (
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );
 	fp=fopen(logfile, "a+");
	fprintf(fp,"# initial	expected	actual	     diff	start	end	exectime	tardiness\n");
	fclose(fp);
  return 0;
}



int static counter=0;
static int settings =2; //0 for litmus 1 for schedfifo 2 for cfs
static struct timespec sleepValue = {0,0};
const long INTERVAL_MS = PERIOD * NANO_SECOND_MULTIPLIER;
int main(int argc, char** argv)
{
	int i;
	struct thread_context ctx[NUM_THREADS];
	pthread_t             task[NUM_THREADS];

	if(settings == 1){
		if(setschedfunc()==0)
		exit(1);
	}
	/* The task is in background mode upon startup. */		


	/*****
	 * 1) Command line paramter parsing would be done here.
	 */
	if(argc < 2) {
    fprintf(stderr, "Usage: enter filename\n");
    exit(1);
  }	
	strcpy(filename, argv[1]);
	strcpy(logfile,"/home/gbaduz/ramfs/");
	strcat(logfile,argv[2]);

       
	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */



	/*****
	 * 3) Initialize LITMUS^RT.
	 *    Task parameters will be specified per thread.
	 */

	pthread_mutex_init(&count_mutex, NULL);
  	pthread_cond_init (&decode_threshold_cv, NULL);          
        pthread_cond_init (&read_threshold_cv, NULL);          
       	if(settings == 0){             
	init_litmus();
	}
	videocode();

	/***** 
	 * 4) Launch threads.
	 */
	i=0;
	ctx[i].id = i;
		pthread_create(task + i, NULL, rt_threaddec, (void *) (ctx + i));
	i++;
	ctx[i].id = i;
		pthread_create(task + i, NULL, rt_threaddis, (void *) (ctx + i));
	
	i++;
	ctx[i].id = i;
		pthread_create(task + i, NULL, rt_threadfetch, (void *) (ctx + i));
	
	
	/*****
	 * 5) Wait for RT threads to terminate.
	 */
	for (i = 0; i < NUM_THREADS; i++)
		pthread_join(task[i], NULL);
	

	/***** 
	 * 6) Clean up, maybe print results and stats, and exit.
	 */
	// Free the YUV frame
  av_free(pFrame);
  
  // Close the codec
  avcodec_close(pCodecCtx);
  
  // Close the video file
  avformat_close_input(&pFormatCtx);
pthread_mutex_destroy(&count_mutex);
  pthread_cond_destroy(&decode_threshold_cv);
	pthread_cond_destroy(&read_threshold_cv);
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

static int done = 0;
static int read1 =0;
static int count=0;
static int end =0;
void* rt_threadfetch(void *tcontext)
{
	
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

	}//end settings

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
		jobfetch();	
	
	} while (counter < 1000);

	
	/*****
	 * 4) Transition to background mode.
	 */
	if(settings == 0){	
	CALL( task_mode(BACKGROUND_TASK) );}


	return NULL;
}
int jobfetch(void) 
{	

	
	pthread_mutex_lock(&count_mutex);
	
	
	
	while(av_read_frame(pFormatCtx, &packet)>=0) {
    // Is this a packet from the video stream?
   	 if(packet.stream_index==videoStream) {
			read1 = 1;
			
			count++;
			pthread_cond_signal(&read_threshold_cv);

			if(end == 1){
				pthread_cond_wait(&end_threshold_cv, &count_mutex);
			}
			end = 1;

			break;
		}
	}
		
	pthread_mutex_unlock(&count_mutex);

	return 0;

  
  
  	
	
}



/* A real-time thread is very similar to the main function of a single-threaded
 * real-time app. Notice, that init_rt_thread() is called to initialized per-thread
 * data structures of the LITMUS^RT user space libary.
 */
static int endtasks=0;
void* rt_threaddec(void *tcontext)
{
	
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
		jobdecode();	
		
	} while (counter < 1000);

	endtasks =1;
	
	/*****
	 * 4) Transition to background mode.
	 */
	if(settings == 0){	
	CALL( task_mode(BACKGROUND_TASK) );}


	return NULL;
}



int jobdecode(void) 
{
	/* Do real-time calculation. */
	//printf("decode");
	/* Don't exit. */


	pthread_mutex_lock(&count_mutex);
  
  

  // Read frames and save first five frames to disk
  i=0;

	 if(read1 == 0){
	pthread_cond_wait(&read_threshold_cv, &count_mutex); //wait as long as read is 0
	}
  	read1 = 0;
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, 
			   &packet);
      
      
    
      if(frameFinished) {
	done =1;
	pthread_cond_signal(&decode_threshold_cv);
	}
	
	pthread_mutex_unlock(&count_mutex);
     
    SDL_PollEvent(&event);
    switch(event.type) {
    case SDL_QUIT:
      SDL_Quit();
      exit(0);
      break;
    default:
      break;
    }
	
	return 0;

	
  
  
  	
	
}


double previous=0.0;
double expectt=0;
double now=0;
double diff=0;
void* rt_threaddis(void *tcontext)
{
	char endfile[50]={0x0};
	FILE * fpe;
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
	
	//be_migrate_to_cluster(1, 1);
	//param.cpu = cluster_to_first_cpu(1, 1);


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
	clock_gettime(CLOCK_MONOTONIC, &tinitial);
	previous = ((double)tinitial.tv_sec + 1.0e-9*tinitial.tv_nsec);

	/*****
	 * 3) Invoke real-time jobs.
	 */
	do {
		/* Wait until the next job is released. */

		
		/* Invoke job. */
		 jobdisplay();	
		if(settings == 0){
			sleep_next_period();
	
		}
		else{	
			sleepValue.tv_nsec = INTERVAL_MS;
			nanosleep(&sleepValue, NULL);
		}
		counter++;
		
	} while (counter < 1000);


	
	/*****
	 * 4) Transition to background mode.
	 */
	if(settings == 0){	
	CALL( task_mode(BACKGROUND_TASK) );}
	sprintf(endfile,"end%d",getpid());
	fpe = fopen(endfile, "ab+");
	fclose(fpe);
	return NULL;
}



int jobdisplay(void) 
{
	/* Do real-time calculation. */
	//printf("display");
	/* Don't exit. */
	// Did we get a video frame?
	FILE  *fp;
	double start;
	double final;
	double exectime;	
	
	pthread_mutex_lock(&count_mutex);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
      {
	if(done == 0){
	pthread_cond_wait(&decode_threshold_cv, &count_mutex); //wait as long as done is 0
	}
	SDL_LockYUVOverlay(bmp);
	done = 0;                                                //reset to 0 since 1 made it escape wait
	
	pict.data[0] = bmp->pixels[0];
	pict.data[1] = bmp->pixels[2];
	pict.data[2] = bmp->pixels[1];

	pict.linesize[0] = bmp->pitches[0];
	pict.linesize[1] = bmp->pitches[2];
	pict.linesize[2] = bmp->pitches[1];

	// Convert the image into YUV format that SDL uses
    sws_scale
    (
        sws_ctx, 
        (uint8_t const * const *)pFrame->data, 
        pFrame->linesize, 
        0,
        pCodecCtx->height,
        pict.data,
        pict.linesize
    );
	
	SDL_UnlockYUVOverlay(bmp);
	
	rect.x = 0;
	rect.y = 0;
	rect.w = 640;
	rect.h = 480;

	 clock_gettime(CLOCK_MONOTONIC, &tinitial);
	now = ((double)tinitial.tv_sec + 1.0e-9*tinitial.tv_nsec);
	expectt = previous*1000.0+ 40;
	diff = (now*1000.0)-expectt;
	
	
	
	SDL_DisplayYUVOverlay(bmp, &rect);//disregard first reading
	
// Free the packet that was allocated by av_read_frame
   av_free_packet(&packet);
	
 	
      }
	
	SDL_PollEvent(&event);
    switch(event.type) {
    case SDL_QUIT:
      SDL_Quit();
      exit(0);
      break;
    default:
      break;
    }
	end = 2;
	pthread_cond_signal(&end_threshold_cv);
	clock_gettime(CLOCK_MONOTONIC, &tfinal);
	pthread_mutex_unlock(&count_mutex);


	start = ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
	final = ((double)tfinal.tv_sec + 1.0e-9*tfinal.tv_nsec);
	exectime = (final-start) * 1000;
	fp=fopen(logfile, "a+");
	fprintf(fp,"%.9f	%.9f	%.9f	%.9f	%.9f	%.9f	%.9f	%.9f\n",(double)previous*1000,(double)expectt,(double)now*1000.0,(double)diff,(double)start,(double)final,(double)exectime,(double)exectime-PERIOD);
	fclose(fp);	
	previous = now;
	return 0;
}
