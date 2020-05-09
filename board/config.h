#ifndef PANDA_JUNGLE_CONFIG_H
#define PANDA_JUNGLE_CONFIG_H

//#define DEBUG
//#define DEBUG_USB

#include "stm32f4xx.h"

#define USB_VID 0xbbaaU

// Different PID than a panda!
#ifdef BOOTSTUB
#define USB_PID 0xddefU
#else
#define USB_PID 0xddcfU
#endif

#include <stdbool.h>
#define NULL ((void*)0)
#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * ((int)(!(pred))))]))

#define MIN(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   (_a < _b) ? _a : _b; })

#define MAX(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   (_a > _b) ? _a : _b; })

#define MAX_RESP_LEN 0x40U

#endif

