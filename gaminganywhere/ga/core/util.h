#ifndef UTIL_H
#define UTIL_H
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif
#include<time.h>
#include "ga-common.h"

EXPORT double diffclock(clock_t clock1,clock_t clock2);
#endif