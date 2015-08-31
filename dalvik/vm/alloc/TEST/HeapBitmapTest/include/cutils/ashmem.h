#ifndef ASHMEM_H_
#define ASHMEM_H_

#include <fcntl.h>

#define ASHMEM_NAME_LEN 128

inline int
ashmem_create_region(const char *name, size_t len)
{
    return open("/dev/zero", O_RDWR);
}

#endif
