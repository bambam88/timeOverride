#ifndef __TIMEOVERRIDE_H_INCL__
#define __TIMEOVERRIDE_H_INCL__

bool init_lib();
bool getErrorMessage(char *txt, uint32_t maxStrSize);
//int gettimeofday(struct timeval *tv, struct timezone *tz) 
//int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int nanosleep(const struct timespec *pRequested_time, struct timespec *pRem);


// Debug flags
#define DBG_API_ENTRY_LEAVE             0x00000001
#define DBG_API_THREAD_MANAGEMENT       0x00000002
#define DBG_API_THREAD_PAUSE            0x00000004
#define DBG_API_THREAD_RESUME           0x00000008


#define DBG_PRINTS_ALL                  0xFFFFFFFF
#define DBG_ALWAYS_PRINT                0x80000000




#define DEBUG_FLAGS_ENABLED (DBG_API_THREAD_PAUSE | DBG_API_THREAD_RESUME)

//#define DEBUGGING

#if defined(DEBUGGING)
#define debug_print(level,fmt, ...) \
        do { if (level & DEBUG_FLAGS_ENABLED) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define debug_print(level,fmt, ...)
#endif




#endif // __TIMEOVERRIDE_H_INCL__

