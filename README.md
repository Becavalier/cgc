# cgc
An experimental Garbage Collector for C language.

### How to use it?

This is a single header implementation, please refer to the main file under the "test" folder. 

Please note that "**THIS IS NOT FOR PRODUCTION USE**".

### Limitation

Actually, as an experimental GC for C, there are a lot of limitations I found during the implementation, please refer below for the categorized details.

#### Platform

This is only for Unix-like x86-64 systems. The way of memory scan is based on certain x86-64 assembly instructions (.e.g `pushfq`) and the way the program memory is organized (ELF).

#### Conservative

This is a conservative GC implementation, due to some hard-to-resolved limitations, the GC itself cannot collect and free all the unused blocks of memory in some cases. For example, this GC would not scan the memory space used by static and global variables on MacOS, since it's hard for me to delineate the boundary of this area. And this area is allocated by the compiler which is even not continuous in the virtual memory space, this is part of the difference in GC implementation between C/C++ and other languages which have a runtime.  

#### Performance

Since we don't have enough type information with respect to the C variables, thus it's hard to find all the possible memory references within a short scanning time. The GC itself needs to scan every byte to determine whether it's a potential memory reference to the heap or not. So, it suffers from a performance issue. 

#### Timing

As a "Stop-the-world" GC, usually, we can decide the timing to run the GC based on the number of allocated objects. But for C, we only know how much memory has been allocated, thus it's hard to define a threshold when to run the GC.

#### Concurrent

This GC implementation doesn't support multi-threading, because it uses global static variables which are shared among all the threads, and also the system interface "sbrk" is not reentrant. Thus, re-call the GC collection method in the middle of the previous GC collection may cause undefined behavior.
