#ifndef _OS_H_
#define _OS_H_

#if STDTHREAD_THREAD
    #include <stdlib/Thread.h>
#else
    #error "Must include thread implementation"
#endif

#endif