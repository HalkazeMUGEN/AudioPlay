#ifndef __AUDIOPLAY_H__
#define __AUDIOPLAY_H__

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#define UNUSED(x) \
  { (void)(x); }

static int32_t (*const TimerWaitSync)(void) = (int32_t(*const)(void))0x482b50;

#endif  // __AUDIOPLAY_H__
