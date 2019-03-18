#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <kernel.h>
#include <nanos.h>

#define DEBUG
#ifdef DEBUG
#include <debug.h>
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif
