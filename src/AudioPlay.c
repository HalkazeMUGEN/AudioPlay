#include <AudioPlay.h>
#include <MCIManager/MCIManager.h>
#include <SyncFPS/SyncFPS.h>
#include <allegro.h>
#include <process.h>
#include <stdio.h>

static wchar_t TEST_MUSIC_PATH[] = L"chars/kfm/Music.mp3";

void init_log(void) {
  FILE* fp;
  AllocConsole();
  SetConsoleOutputCP(CP_UTF8);
  UNUSED(freopen_s(&fp, "CONOUT$", "w", stdout));
  UNUSED(freopen_s(&fp, "CONOUT$", "w", stderr));
}

void MCIM_CALLBACK(MCIM_NOTIFY_FLAGS flag) {
  printf("MCIM_CALLBACK: flag = %d\n", flag);
}

void MCIM_FADEOUT_CALLBACK(MCIM_NOTIFY_FLAGS flag) {
  printf("MCIM_FADEOUT_CALLBACK: flag = %d\n", flag);
}

void MCIM_WAIT(void) {
  static SYNC_FPS_DATA* fps = NULL;
  if (fps == NULL) {
    fps = init_sync_fps(60.0);
  }
  wait_sync_fps(fps);
}

void func(void* pargs) {
  UNUSED(pargs);

  SYNC_FPS_DATA* fps = init_sync_fps(60.0);
  LARGE_INTEGER mTimeFreq;
  LARGE_INTEGER mTimeStart;
  LARGE_INTEGER mTimeEnd;
  QueryPerformanceFrequency(&mTimeFreq);
  while (true) {
    QueryPerformanceCounter(&mTimeStart);
    // TimerWaitSync();
    wait_sync_fps(fps);
    QueryPerformanceCounter(&mTimeEnd);
    // mFrameTime = フレームごとの間隔（単位：秒）
    double mFrameTime = (double)(mTimeEnd.QuadPart - mTimeStart.QuadPart) / (double)(mTimeFreq.QuadPart);
    printf("T = %.15f, FPS = %f\n", mFrameTime, 1.0 / mFrameTime);
  }

  free_sync_fps(fps);
}

void test_mcim(void* pargs) {
  UNUSED(pargs);

  MCIM_DATA* mcim = mcim_init(win_get_window());
  if (mcim == NULL) {
    printf("mcim_init => NULL\n");
    return;
  }
  MCIM_KEY key = mcim_load(mcim, TEST_MUSIC_PATH);
  if (key == MCIM_INVALID_KEY) {
    printf("mcim_load => MCIM_INVALID_KEY\n");
    return;
  }
  if (!mcim_play(mcim, key, MCIM_CALLBACK)) {
    printf("mcim_play => false\n");
    return;
  }
  /*
  if (mcim_fadeout(mcim, key, NULL, 300, MCIM_FADEOUT_CALLBACK) == MCIM_INVALID_KEY) {
    printf("mcim_fadeout => MCIM_INVALID_KEY\n");
    return;
  }
  */
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
  UNUSED(hinstDLL);
  UNUSED(lpReserved);
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      init_log();
      _beginthread(test_mcim, 0, NULL);
      //_beginthread(func, 0, NULL);
      break;
    default:
      break;
  }
  return TRUE;
}
