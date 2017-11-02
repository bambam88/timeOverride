#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <map>
#include <list>


using namespace std;

#include "timeOverride.h"


typedef struct timeval PX4_TIMEVAL, *pPX4_TIMEVAL;

static int (*orig_clock_gettime)(clockid_t, struct timespec *) = NULL;
static int (*orig_nanosleep)(struct timespec *, struct timespec *) = NULL;
static int (*orig_gettimeofday)(struct timeval *, struct timezone *) = NULL;

static struct timespec timebase_monotonic;
static struct timespec timebase_realtime;
static struct timeval timebase_gettimeofday;

typedef struct _THREAD_INFO_TAG {
    uint64_t timeout;
    pthread_mutex_t *pThreadMutex;
    pthread_cond_t  *pResumeCondition;

}THREAD_INFO, *pTHREAD_INFO;


typedef enum R_STATUS {
    STATUS_SUCCESS = 0,
    STATUS_UNINITIALIZED_LIB = INT_MIN,
    STATUS_INVALID_TIMEOUT,
    STATUS_INVALID_MULTIPLIER,
    STATUS_UNKNOWN_ERROR

}RETURN_STATUS;


PX4_TIMEVAL clock_gpsTime;

pthread_mutex_t CLOCK_GPSTIME_lock;
pthread_mutex_t threadMutex;
pthread_cond_t  resumeCondition;


static std::list<pTHREAD_INFO> wakeup_tasks;

// We dont really need to initialize the library, 
// but just in case we will use the system nanosleep at all,
// here they are...just need to call this function first
bool init_lib() {
    debug_print(DBG_API_ENTRY_LEAVE, "Entering init_lib %s", "\n");

    // Not sure that we are going to need these, but
    // here they are just in case
    // get the system nanosleep function
    orig_nanosleep      = (int (*)(struct timespec *, struct timespec *))dlsym(RTLD_NEXT, "nanosleep");
    orig_clock_gettime  = (int (*)(clockid_t, struct timespec *))dlsym(RTLD_NEXT, "clock_gettime");
    orig_gettimeofday   = (int (*)(struct timeval *, struct timezone *))dlsym(RTLD_NEXT, "clock_gettimeofday");

    debug_print(DBG_API_ENTRY_LEAVE, "Leaving init_lib %s", "\n");
}

bool getErrorMessage(char *txt, uint32_t maxStrSize) {

    //TODO: Add code to fill a descrfiptive value for the error code

}

void pauseThread(pTHREAD_INFO pThr) {
    static uint64_t count  = 0;
    	

    debug_print(DBG_API_ENTRY_LEAVE, "Entering Pause%s", "\n");

    debug_print(DBG_API_THREAD_PAUSE, "Pausing Thread%s", "\n");
    

    // tell the thread to suspend
    pthread_mutex_lock(pThr->pThreadMutex);
    count++;

    pthread_cond_wait(pThr->pResumeCondition, pThr->pThreadMutex);

    count--;
    pthread_mutex_unlock(pThr->pThreadMutex);


    debug_print(DBG_API_THREAD_PAUSE, "Thread is resumed, count remain (%lu)\n", count);

    debug_print(DBG_API_ENTRY_LEAVE, "Leaving Pause%s", "\n");
}

void resumeThread(pTHREAD_INFO pThr) {

    debug_print(DBG_API_ENTRY_LEAVE, "Entering resume %s", "\n");

    // tell the thread to resume
    pthread_mutex_lock(pThr->pThreadMutex);
    pthread_cond_broadcast(pThr->pResumeCondition);
    pthread_mutex_unlock(pThr->pThreadMutex);

    debug_print(DBG_API_ENTRY_LEAVE, "Leaving resume %s", "\n");
}




int gettimeofday(struct timeval *tv, struct timezone *tz) {
    int status = 0; //ERROR_SUCCESS;

    debug_print(DBG_API_ENTRY_LEAVE, "Entering getTime %s", "\n");



        pthread_mutex_lock(&CLOCK_GPSTIME_lock);
        *tv = clock_gpsTime;
        pthread_mutex_unlock(&CLOCK_GPSTIME_lock);



    debug_print(DBG_API_ENTRY_LEAVE, "Leaving getTimeOfDay (%d)\n", status);
    return status;
}




// called each time TimeGps updates
int settimeofday(const struct timeval *tv, const struct timezone *tz) {

    int status = 0;
    debug_print(DBG_API_ENTRY_LEAVE, "Entering setTimeOfDay %s", "\n");
    pthread_mutex_lock(&CLOCK_GPSTIME_lock);
    clock_gpsTime = *tv;


    uint64_t tv64 = clock_gpsTime.tv_sec * 1000000000 + clock_gpsTime.tv_usec * 1000000;

    debug_print(DBG_API_THREAD_MANAGEMENT, "task_count is %lu\n", wakeup_tasks.size());
   pTHREAD_INFO pThisTask;

   for (std::list<pTHREAD_INFO>::iterator it=wakeup_tasks.begin(); it != wakeup_tasks.end();)
        {
            pThisTask = (pTHREAD_INFO)(*it);
            debug_print(DBG_API_THREAD_MANAGEMENT, "Testing timeout(%lu)<time(%lu)\n", pThisTask->timeout,tv64);
            if (pThisTask->timeout <= tv64)
            {
                debug_print(DBG_API_THREAD_RESUME, "Resuming Paused Thread%s", "\n");
                resumeThread(pThisTask);

                // cleanup
//                delete(pThisTask->pResumeCondition);
//                delete(pThisTask->pThreadMutex);
//               delete(pThisTask);
                it = wakeup_tasks.erase(it);
            }
            else
            {
                ++it;
            }
        }



    pthread_mutex_unlock(&CLOCK_GPSTIME_lock);

    debug_print(DBG_API_ENTRY_LEAVE, "Leaving setTimeOfDay (%d)\n", status);

    return status;
}

int nanosleep(const struct timespec *pRequested_time, struct timespec *pRem) {

    struct timeval tBefore, tNow, tAfter;
    int errno;

    pthread_mutex_t mtx;
    pthread_cond_t cond;
    THREAD_INFO thr;

    debug_print(DBG_API_ENTRY_LEAVE, "Entering nanosleep %s", "\n");

          
    gettimeofday(&tNow, NULL);

    uint64_t texp = (pRequested_time->tv_sec * 1000000000) + pRequested_time->tv_nsec;
    texp += (tNow.tv_sec * 1e+9) + tNow.tv_usec * 1e+6;

    debug_print(DBG_API_THREAD_MANAGEMENT, "\t\tRequested (%lu), Now (%lu):Expires in (%lu)\n", 
                (pRequested_time->tv_sec * 1000000000) + pRequested_time->tv_nsec, 
                (tNow.tv_sec * 1000000000) + tNow.tv_usec * 1000000, 
                texp);


    // Validate the timeout value
    if (pRequested_time->tv_sec < 0     ||
        pRequested_time->tv_nsec < 0    ||
        pRequested_time->tv_nsec >= 1000000000) {
        errno = EINVAL;
        debug_print(DBG_API_ENTRY_LEAVE, "Invalid timeout value (%d)\n", errno);

    } else {

        // if we assume that TimeGps is faster than wall-clock time,
        // then we can ignore the possibility that wall-clock will expire before we do…
        pthread_mutex_lock(&CLOCK_GPSTIME_lock);


        pTHREAD_INFO pThisTask = &thr; //new (THREAD_INFO);
        pThisTask->timeout  =   texp;
        pThisTask->pThreadMutex = &mtx;//new (pthread_mutex_t);
        pThisTask->pResumeCondition = &cond; //new (pthread_cond_t);

        // initialize the condition with the default params
        pthread_mutex_init(pThisTask->pThreadMutex, NULL);
        pthread_cond_init (pThisTask->pResumeCondition, NULL);;


        wakeup_tasks.push_back(pThisTask);

        pthread_mutex_unlock(&CLOCK_GPSTIME_lock);
        debug_print(DBG_API_THREAD_MANAGEMENT, "Adding new thread to the collection, count (%lu)\n", wakeup_tasks.size());


        // pause the calling thread

        // errno = pause();

        // will use our own pause/resume functions, pause() seems
        // to have problems with large number of threads
        //        pauseThread(pThisTask);
        pauseThread(wakeup_tasks.back());
        // Stay here until resumed -- presumably by the settimeofday function
    }
    debug_print(DBG_API_ENTRY_LEAVE, "Leaving nanosleep (%d)\n", errno);
    return errno;

}

