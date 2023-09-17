#include "_SyncFPS.h"

SYNC_FPS_DATA* init_sync_fps_al(double fps, sync_fps_allocator_t allocator, sync_fps_deallocator_t deallocator) {
  if (fps <= 0.0 || allocator == NULL || deallocator == NULL) {
    return NULL;
  }

  SYNC_FPS_DATA_INTERNAL* ret = (SYNC_FPS_DATA_INTERNAL*)allocator(sizeof(SYNC_FPS_DATA_INTERNAL));
  if (ret == NULL) {
    return NULL;
  }

  ret->fps = fps;
  ret->period = 1.0 / fps;

  // 以下三つの関数はWindows Vista以降は失敗しないため、エラー処理は不要
  QueryPerformanceFrequency(&(ret->timerFreq));
  QueryPerformanceCounter(&(ret->timerStart));
  InitializeCriticalSection(&(ret->mutex));

  ret->allocator = allocator;
  ret->deallocator = deallocator;

  return (SYNC_FPS_DATA*)ret;
}

bool wait_sync_fps(SYNC_FPS_DATA* data) {
  if (data == NULL) {
    return false;
  }

  SYNC_FPS_DATA_INTERNAL* d = (SYNC_FPS_DATA_INTERNAL*)data;

  if (TryEnterCriticalSection(&(d->mutex)) == 0) {
    EnterCriticalSection(&(d->mutex));
    LeaveCriticalSection(&(d->mutex));
    return true;
  }

  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  double time = (double)(t.QuadPart - (d->timerStart).QuadPart) / (double)((d->timerFreq).QuadPart);
  if (time < d->period) {
    DWORD sleepTime = (DWORD)((d->period - time) * 1000);
    timeBeginPeriod(1);
    Sleep(sleepTime);
    timeEndPeriod(1);
  }
  QueryPerformanceCounter(&(d->timerStart));

  LeaveCriticalSection(&(d->mutex));
  return true;
}

bool free_sync_fps(SYNC_FPS_DATA* data) {
  if (data == NULL) {
    return true;
  }

  SYNC_FPS_DATA_INTERNAL* d = (SYNC_FPS_DATA_INTERNAL*)data;
  DeleteCriticalSection(&(d->mutex));
  SecureZeroMemory(&(d->mutex), sizeof(CRITICAL_SECTION));

  d->deallocator(data);
  return true;
}
