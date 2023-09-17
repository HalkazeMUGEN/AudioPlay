#ifndef ___SYNCFPS_H__
#define ___SYNCFPS_H__

#include "SyncFPS/SyncFPS.h"

#include <assert.h>
#include <windows.h>

typedef struct SYNC_FPS_DATA_INTERNAL {
  double fps;
  double period;
  LARGE_INTEGER timerFreq;
  LARGE_INTEGER timerStart;
  CRITICAL_SECTION mutex;
  sync_fps_allocator_t allocator;
  sync_fps_deallocator_t deallocator;

} SYNC_FPS_DATA_INTERNAL;

#endif  // ___SYNCFPS_H__
