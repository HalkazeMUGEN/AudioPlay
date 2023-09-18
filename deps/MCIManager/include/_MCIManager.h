#ifndef ___MCIMANAGER_H__
#define ___MCIMANAGER_H__

#include "MCIManager/MCIManager.h"
#include "uthash.h"

#include <digitalv.h>
#include <stdatomic.h>

// MSVCではAtomicの一部機能が未実装であり、
// 定数以外（const変数を含む）でAtomic変数を初期化できないため
#define MCIM_INITIAL_KEY_VALUE 0

typedef enum _MCIM_STATUS {
  MCIM_STATUS_UNLOADED = 0,
  MCIM_STATUS_LOADED = 1,
  MCIM_STATUS_PLAYING = 2,
  MCIM_STATUS_FADINGOUT = 3
} MCIM_STATUS;

typedef struct _MCIM_MUSIC_ENTRY {
  MCIM_KEY key;
  MCIDEVICEID id;
  MCIM_STATUS status;
  uint32_t volume;
  wchar_t* filepath;
  struct _MCIM_MUSIC_ENTRY* next;
} MCIM_MUSIC_ENTRY;

typedef struct _MCIM_THREAD_DATA {
  HANDLE hthread;
  uint32_t threadId;
  HANDLE initializedEvent;
  atomic_uintptr_t wait;
} MCIM_THREAD_DATA;

typedef struct _MCIM_DATA_INTERNAL {
  MCIM_MUSIC_ENTRY* bgmlist;
  HWND hwnd;
  mcim_allocator_t allocator;
  mcim_deallocator_t deallocator;
  MCIM_THREAD_DATA th;
} MCIM_DATA_INTERNAL;

typedef struct _MCIM_CALLBACK_TABLE_ENTRY {
  MCIDEVICEID id;
  MCIM_CALLBACK_PROC callback;
  UT_hash_handle hh;
} MCIM_CALLBACK_TABLE_ENTRY;

typedef enum _MCIM_THREAD_MESSAGES {
  MCIM_TM_TERMINATE = (WM_USER + 0x10),
  MCIM_TM_FADEOUT
} MCIM_THREAD_MESSAGES;

#endif  // ___MCIMANAGER_H__
