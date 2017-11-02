/**
    WorldView
    main.cpp
    Purpose: Create simple threads that sleep for a user specified time

    Arguments:
    -c: number of threads to generate
    -s: timeout it mS
    -t: time between clock ticks

    @author jyanez
    @version 0.1 10/28/16
*/

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <cstdlib>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>       /* for mktime()       */
#include <signal.h>
#include <errno.h>
#include <vector>


#ifdef DEBUG

#define dbgprint(x) cerr << #x << ": " << x << " in "  << __FILE__ << ":" << __LINE__ << endl
#define dbgmsg(message) cerr << message " in "  << __FILE__ << ":" << __LINE__ << endl

#else

#define dbgprint(x)
#define dbgmsg(message)

#endif

using namespace std;

typedef struct timeval  TIME_VALUE, *pTIME_VALUE;
typedef struct timezone TIMEZONE, TIMEZONE;
typedef double timestamp_t; /* Unix time in seconds with fractional part */

struct globalArgs_t {
    uint16_t numThreads;        /* -X option */
    uint64_t sleepTimeout;		/* -Y option -- in nanoseconds */
    uint64_t tickTime;		/* -Y option -- in nanoseconds */
    int verbosity;				/* -v option */
} globalArgs;

static const char *optString = ":c:s:t:v:h:?:";

bool threadsAreRunning;


/**
    Generates a sleep timeout in nanoseconds

    @param milliseconds -- timeout in milliseconds
    @return result from calling nanosleep

*/

int msleep(long miliseconds)
{
   struct timespec req, rem;

   if(miliseconds > 999)
   {
        req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
        req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
   }
   else
   {
        req.tv_sec = 0;                         /* Must be Non-Negative */
        req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
   }

   return nanosleep(&req , &rem);
}

/**
    Calls msleep with.

    @param *t Pointer to a user argument
    @return none

*/
void *wait(void *t) {

   msleep(globalArgs.sleepTimeout);
   pthread_exit(NULL);
}

/**
    Generates a clock tick by calling settimeofDay.
    Additionally, the function implements a simple
    date/time clock. No support for leap years


    @param timevalue to pass in to settimeofday
    @return nothing really
*/
int tickClock(struct tm *timeVal) {
    int status = 0;
    static int sec = 0;         // 00:00:00 --- yikes
    static int mins = 0;
    static int hrs = 0;
    static int mday = 1;        // first day of the month
    static int mon = 1;         // starts in january
    static int year = 2017 - 1900;

    // clock tick
    if (++sec > 59) {
        sec = 0;
        if (++mins > 59) {
            mins = 0;
            if (++hrs > 23) {
                hrs = 0;
                if (++mday > 31) {
                    mday = 1;
                    if (++mon > 12) {
                        mon = 1;
                        year++;
                    }
                }
            }
        }
    }

    timeVal->tm_sec = sec;
    timeVal->tm_min = mins;
    timeVal->tm_hour = hrs;
    timeVal->tm_mday = mday;
    timeVal->tm_mon = mon;
    timeVal->tm_year = year;

    status = setenv("TZ", "UTC", 1);


    if (status < 0) {
    	dbgmsg("setenv() returned an error");

    }

    return status;
}

/**
    Thread that generates tick events

    @param none
    @return none
*/
void *timeTickEvents(void *){
    TIME_VALUE  now;

    time_t new_time;
    int result, savedError;
    struct tm currentTime = {0};

    do
    {
        tickClock(&currentTime);

        new_time = mktime(&currentTime);

        now.tv_sec = new_time;
        now.tv_usec = 0;

        // set the new time
//        cout << "Setting new time to " << new_time << endl;
        result = settimeofday(&now, NULL);



        if (result) {

            savedError = errno;

            if (savedError == EPERM) {
            	dbgmsg("You must run this as root.\n");

            } else {
            	dbgmsg("settimeofday() returned an error:"<<result);


            }
        }


//        volatile int loopCnt = 0;

//        for(loopCnt = 0; loopCnt < 20000; loopCnt++);

        usleep(globalArgs.tickTime);
    } while (threadsAreRunning);

    dbgmsg("Tick generation has ended");

    return &result;

}




/* Display program usage, and exit.
 */
void display_usage( void )
{
    cout << "SleepingThreadsTest - test the use of a simulated time base";
    cout << "\n\tUsage:\n\tSleepingThreadsTest  -c -s -t -v -h\n\tc: Number of threads to generate\n\ts: Sleeping Timeout\n\tt: time between tick events\n";

    exit( EXIT_FAILURE );
}

void getParameters(int argc, char* argv[])
{
    int opt = 0;


    /* Initialize globalArgs before we get to work. */
        globalArgs.numThreads = 1;              //One thread
        globalArgs.sleepTimeout = 1000;   // sleep for one second
        globalArgs.verbosity = 0;               // no verbosity

        /* Process the arguments with getopt(), then
         * populate globalArgs.
         */
        opt = getopt( argc, argv, optString );
        while( opt != -1 ) {
            switch( opt ) {
                case 'c':
                    globalArgs.numThreads = atoi(optarg);	/* true */

                    cout << "NumThreads is " << globalArgs.numThreads << endl;
                    break;

                case 's':
                    globalArgs.sleepTimeout = atol(optarg);
                    cout << "sleepTimeout is " << globalArgs.sleepTimeout << endl;
                    break;

                case 't':
                    globalArgs.tickTime = atol(optarg);
                    cout << "Tick time is " << globalArgs.tickTime << endl;
                    break;

                case 'v':
                    globalArgs.tickTime= atol(optarg);
                    cout << "Tick time is " << globalArgs.tickTime << endl;
                    break;

                case 'h':	/* fall-through is intentional */
                case '?':
                    display_usage();
                    break;

                default:
                    /* You won't actually get here. */
                    break;
            }

            opt = getopt( argc, argv, optString );
        }


}

int main(int argc, char* argv[])
{
    getParameters(argc, argv);

    int rc;
    int i;
    std::vector<pthread_t *> pThreads;
    pthread_t *pThread;
    pthread_t timeThread;
    pthread_attr_t attr;
    void *status;

    // Initialize and set threads joinable
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    threadsAreRunning = true;

    // create thread that will create the tick events
     pthread_create(&timeThread, NULL, timeTickEvents, NULL);


    for( i = 0; i < globalArgs.numThreads; i++ ) {
          usleep(10);
 //      cout << "main() : creating thread, " << i << endl;
        pThread = new (pthread_t);
        rc = pthread_create(pThread, &attr, wait,NULL );
        pThreads.push_back(pThread);

       if (rc) {
          cout << "Error:unable to create thread," << rc << endl;
          exit(-1);
       }
    }

    // free attribute and wait for the other threads
    pthread_attr_destroy(&attr);
    for( i = 0; i < globalArgs.numThreads; i++ ) {
       rc = pthread_join(*pThreads[i], &status);
       if (rc) {
          cout << "Error:unable to join tid(" << i <<"):" << rc << endl;
          exit(-1);
       }

       delete (pThreads[i]);
       cout << "Main: completed thread id :" << i ;
       cout << "  exiting with status :" << status << endl;
    }

     threadsAreRunning = false;

    pthread_join(timeThread, &status);

    cout << "Main: program exiting." << endl;
    pthread_exit(NULL);


    return 0;
}

