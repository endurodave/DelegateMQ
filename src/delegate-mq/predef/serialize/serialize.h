#ifndef _SERIALIZE_H_
#define _SERIALIZE_H_

#if MSGPACK_SERIALIZE
    #include <msgpack/serializer.h>
#elif MSGSERIALIZE_SERIALIZE
    #include <serialize/serializer.h>
#else
    #error "Must include a serialize implementation"
#endif

#endif