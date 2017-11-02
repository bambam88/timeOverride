# timeOverride
Project contains a library and a test program

use "make" to build the library

to test navigate to the location of the generated "so" file.

Run as 

LD_PRELOAD=$PWD/timeOverride.so <Location of driver program>


To test with the included application run as

LD_PRELOAD=$PWD/timeOverride.so ../eSleepingThreads/Debug/eSleepingThreads -c1000 -s100000 -t100

In the example, the thread count is 1000, sleep timeout is 10000ms and the time between ticks is 100us


