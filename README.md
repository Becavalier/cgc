# cgc
An experimental Garbage Collector for C language.

### How to use it?

This is a single header implementation, please refer to the case under the "test" folder. 

Please note that "**THIS IS NOT FOR PRODUCTION USE**".

### Limitation

As an experimental GC for C, actually, there are a lot of limitations I found during the implementation, please refer below for the categorized details.

#### Platform

This is only for Unix-like x86-64 systems. The way of memory scan is based on some certain x86-64 assembly code (.e.g pushfq) and the way the program memory is organized (ELF).

#### Conservative

This is a conservative GC implementation, due to some hard-to-resolved limitations, the GC itself cannot collect and free all the unused blocks of memory in some cases. For example, this GC would not scan the memory space used by static and global variables on MacOS, since it's hard for us to delineate the boundary of this area. And this area is allocated by the compiler which is even not continuous. 

#### Performance

Since we don't have enough type information with respect to the C variables, thus it's hard to find all the possible memory references within a short scanning time. The GC itself needs to scan every byte to determine whether it's a memory reference to the heap or not. So, it suffers from a performance issue. 

#### Concurrent

This GC implementation doesn't support multi-threading, because it uses global static variables which are shared among all the threads, and also the system interface "sbrk" is not reentrant.
