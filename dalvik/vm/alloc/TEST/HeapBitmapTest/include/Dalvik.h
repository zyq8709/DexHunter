#ifndef DALVIK_H_
#define DALVIK_H_

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define ALOGW(...) printf("W/" __VA_ARGS__)
#define ALOGE(...) printf("E/" __VA_ARGS__)

inline void dvmAbort(void) {
    exit(1);
}

#endif  // DALVIK_H_
