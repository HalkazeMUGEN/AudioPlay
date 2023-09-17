#include "_MCIManager.h"

#include <assert.h>
#include <process.h>

/**************************************************************************************************/

static _Atomic MCIM_KEY MCIM_NEXT_KEY = MCIM_INITIAL_KEY_VALUE;

static MCIM_CALLBACK_TABLE_ENTRY* MCIM_CALLBACK_TABLE = NULL;

static uint32_t MCIM_INSTANCE_COUNT = 0;
static HHOOK MCIM_HOOK = NULL;

static atomic_flag MCIM_MUTEX_INITIALIZED = ATOMIC_FLAG_INIT;
static CRITICAL_SECTION MCIM_CALLBACK_TABLE_MUTEX = {0};
static CRITICAL_SECTION MCIM_INSTANCE_COUNT_MUTEX = {0};

/**************************************************************************************************/

_VCRT_RESTRICT MCIM_MUSIC_ENTRY* mcim_create_loaded_entry(const wchar_t* filepath, mcim_allocator_t allocator, mcim_deallocator_t deallocator);
_VCRT_NOALIAS static bool mcim_entry_is_playing(const MCIM_MUSIC_ENTRY* entry);
_VCRT_NOALIAS static bool mcim_entry_equal(const MCIM_MUSIC_ENTRY* entry, const wchar_t* filepath);
_VCRT_NOALIAS static MCIM_NOTIFY_FLAGS mcim_convert_flag(uint32_t mci_flag);

static MCIM_KEY mcim_load_entry(MCIM_MUSIC_ENTRY* entry);
static bool mcim_unload_entry(MCIM_MUSIC_ENTRY* entry, mcim_deallocator_t deallocator);
static bool mcim_play_entry(MCIM_MUSIC_ENTRY* entry,
                            int32_t from,
                            MCIM_CALLBACK_PROC callback,
                            HWND callbackWindow,
                            mcim_allocator_t allocator,
                            mcim_deallocator_t deallocator);
static bool mcim_stop_entry(MCIM_MUSIC_ENTRY* entry, mcim_deallocator_t deallocator);
static bool mcim_fadeout_entry(MCIM_MUSIC_ENTRY* restrict entry,
                               MCIM_THREAD_DATA* restrict th,
                               MCIM_WAIT_NEXT_FRAME wait,
                               int32_t time,
                               MCIM_CALLBACK_PROC callback,
                               mcim_allocator_t allocator,
                               mcim_deallocator_t deallocator);

static LRESULT CALLBACK mcim_callback(int code, WPARAM wParam, LPARAM lParam);
static MCIM_CALLBACK_PROC mcim_find_callback(MCIDEVICEID id);
static bool mcim_add_callback_table(MCIDEVICEID id, MCIM_CALLBACK_PROC callback, mcim_allocator_t allocator);
static void mcim_del_callback_table(MCIDEVICEID id, mcim_deallocator_t deallocator);

static bool mcim_command_open(MCIDEVICEID* restrict pId, const wchar_t* restrict filepath);
static bool mcim_command_get_volume(MCIDEVICEID id, uint32_t* pVolume);
static bool mcim_command_set_volume(MCIDEVICEID id, uint32_t volume);
static bool mcim_command_play(MCIDEVICEID id);
static bool mcim_command_play_callback(MCIDEVICEID id, HWND callbackWindow);
static bool mcim_command_play_from(MCIDEVICEID id, int32_t from);
static bool mcim_command_stop(MCIDEVICEID id);
static bool mcim_command_close(MCIDEVICEID id);

static bool mcim_create_fadeout_thread(MCIM_DATA_INTERNAL* data);
static bool mcim_start_fadeout_thread(MCIM_THREAD_DATA* restrict th, const MCIM_MUSIC_ENTRY* restrict entry, MCIM_WAIT_NEXT_FRAME wait, int32_t time);
static void mcim_terminate_fadeout_thread(MCIM_THREAD_DATA* th);
static unsigned __stdcall mcim_fadeout_thread(void* pargs);

/**************************************************************************************************/

MCIM_DATA* mcim_init_al(HWND callbackWindow, void* (*allocator)(size_t), void (*deallocator)(void*)) {
  if (callbackWindow == INVALID_HANDLE_VALUE || allocator == NULL || deallocator == NULL) {
    return NULL;
  }

  MCIM_DATA_INTERNAL* ret = (MCIM_DATA_INTERNAL*)allocator(sizeof(MCIM_DATA_INTERNAL));
  if (ret == NULL) {
    return NULL;
  }

  SecureZeroMemory(ret, sizeof(MCIM_DATA_INTERNAL));
  ret->bgmlist = NULL;
  ret->hwnd = callbackWindow;
  ret->allocator = allocator;
  ret->deallocator = deallocator;

  if (!mcim_create_fadeout_thread(ret)) {
    deallocator(ret);
    return NULL;
  }

  if (!atomic_flag_test_and_set(&MCIM_MUTEX_INITIALIZED)) {
    InitializeCriticalSectionAndSpinCount(&MCIM_CALLBACK_TABLE_MUTEX, 0);
    InitializeCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);
  }

  // MCIM_INSTANCE_COUNTの操作はatomicだけでは対処できない
  // ∵ MCIM_HOOKの初期化に失敗した場合にMCIM_INSTANCE_COUNTの巻き戻しが入るため、
  //   他のMCIM_INSTANCE_COUNTの参照は少なくともMCIM_INSTANCE_COUNT++～MCIM_INSTANCE_COUNT = 0の間は待機する必要がある
  // また、排他制御する都合上、MCIM_INSTANCE_COUNT自体はatomicにする必要が無くなる
  EnterCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);
  if ((MCIM_INSTANCE_COUNT++) == 0) {
    assert(MCIM_HOOK == NULL);

    DWORD threadId = GetWindowThreadProcessId(callbackWindow, NULL);
    MCIM_HOOK = SetWindowsHookExW(WH_GETMESSAGE, mcim_callback, NULL, threadId);
    if (threadId == 0 || MCIM_HOOK == NULL) {
      MCIM_INSTANCE_COUNT = 0;
      LeaveCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);
      mcim_terminate_fadeout_thread(&(ret->th));
      deallocator(ret);
      return NULL;
    }
  }
  LeaveCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);

  return (MCIM_DATA*)ret;
}

bool mcim_exit(MCIM_DATA* data) {
  if (data == NULL) {
    return true;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;

  // fadeout_thread中でentryを参照しているため、
  // entryの削除はthread削除後である必要がある
  mcim_terminate_fadeout_thread(&(d->th));

  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  if (entry != NULL) {
    do {
      if (!mcim_unload_entry(entry, d->deallocator)) {
        return false;
      }

      // entry->filepathは基本的にNULLにはならないためNULLチェックは省略
      d->deallocator(entry->filepath);

      MCIM_MUSIC_ENTRY* temp = entry->next;
      entry->next = NULL;
      d->deallocator(entry);
      entry = temp;
    } while (entry != NULL);
    d->bgmlist = NULL;
  }
  d->deallocator(data);

  // グローバルへの影響を抑えるため、
  // インスタンスが一つもないときはメッセージのHookを解除する
  EnterCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);
  uint32_t count = --MCIM_INSTANCE_COUNT;
  LeaveCriticalSection(&MCIM_INSTANCE_COUNT_MUTEX);
  if (count == 0) {
    assert(MCIM_HOOK != NULL);
    if (UnhookWindowsHookEx(MCIM_HOOK) == 0) {
      return false;
    }
    MCIM_HOOK = NULL;
  }

  return true;
}

MCIM_KEY mcim_load(MCIM_DATA* data, const wchar_t* filepath) {
  if (data == NULL || filepath == NULL || filepath[0] == L'\0') {
    return MCIM_INVALID_KEY;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY** pentry = &(d->bgmlist);

  while (*pentry != NULL) {
    if (mcim_entry_equal(*pentry, filepath)) {
      return mcim_load_entry(*pentry);
    }

    pentry = &((*pentry)->next);
  }

  MCIM_MUSIC_ENTRY* new_entry = mcim_create_loaded_entry(filepath, d->allocator, d->deallocator);
  if (new_entry == NULL) {
    return MCIM_INVALID_KEY;
  }
  *pentry = new_entry;
  return new_entry->key;
}

bool mcim_unload(MCIM_DATA* data, MCIM_KEY key) {
  if (data == NULL) {
    return false;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  while (entry != NULL) {
    if (entry->key == key) {
      return mcim_unload_entry(entry, d->deallocator);
    }
    entry = entry->next;
  }

  return false;
}

bool mcim_play(MCIM_DATA* data, MCIM_KEY key, MCIM_CALLBACK_PROC callback) {
  if (data == NULL) {
    return false;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  while (entry != NULL) {
    if (entry->key == key) {
      return mcim_play_entry(entry, 0, callback, d->hwnd, d->allocator, d->deallocator);
    }
    entry = entry->next;
  }
  return false;
}

bool mcim_play_from(MCIM_DATA* data, MCIM_KEY key, int32_t from) {
  if (data == NULL) {
    return false;
  }

  if (from < 0) {
    from = 0;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  while (entry != NULL) {
    if (entry->key == key) {
      return mcim_play_entry(entry, from, NULL, d->hwnd, d->allocator, d->deallocator);
    }
    entry = entry->next;
  }
  return false;
}

MCIM_KEY mcim_stop(MCIM_DATA* data, MCIM_KEY key) {
  if (data == NULL) {
    return MCIM_INVALID_KEY;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  while (entry != NULL) {
    if (entry->key == key || (key == MCIM_MASTER_KEY && mcim_entry_is_playing(entry))) {
      if (mcim_stop_entry(entry, d->deallocator)) {
        assert(entry->key != MCIM_INVALID_KEY);
        return entry->key;
      } else {
        return MCIM_INVALID_KEY;
      }
    }
    entry = entry->next;
  }
  return MCIM_INVALID_KEY;
}

MCIM_KEY mcim_fadeout(MCIM_DATA* data, MCIM_KEY key, MCIM_WAIT_NEXT_FRAME wait, int32_t time, MCIM_CALLBACK_PROC callback) {
  if (data == NULL || wait == NULL || time < 0) {
    return MCIM_INVALID_KEY;
  }

  MCIM_DATA_INTERNAL* d = (MCIM_DATA_INTERNAL*)data;
  MCIM_MUSIC_ENTRY* entry = d->bgmlist;
  while (entry != NULL) {
    if (entry->key == key || (key == MCIM_MASTER_KEY && mcim_entry_is_playing(entry))) {
      if (mcim_fadeout_entry(entry, &(d->th), wait, time, callback, d->allocator, d->deallocator)) {
        assert(entry->key != MCIM_INVALID_KEY);
        return entry->key;
      } else {
        return MCIM_INVALID_KEY;
      }
    }
    entry = entry->next;
  }
  return MCIM_INVALID_KEY;
}

/**********************************************************/

_VCRT_RESTRICT MCIM_MUSIC_ENTRY* mcim_create_loaded_entry(const wchar_t* filepath, mcim_allocator_t allocator, mcim_deallocator_t deallocator) {
  assert(filepath != NULL);
  assert(allocator != NULL);
  assert(deallocator != NULL);

  size_t pathlen = wcslen(filepath);
  wchar_t* path = (wchar_t*)allocator(sizeof(wchar_t) * (pathlen + 1));
  if (path == NULL) {
    return NULL;
  }
  if (wcscpy_s(path, pathlen + 1, filepath) != 0) {
    deallocator(path);
    return NULL;
  }

  MCIDEVICEID id;
  if (!mcim_command_open(&id, path)) {
    deallocator(path);
    return NULL;
  }

  uint32_t volume;
  if (!mcim_command_get_volume(id, &volume)) {
    mcim_command_close(id);
    deallocator(path);
    return NULL;
  }

  MCIM_MUSIC_ENTRY* entry = (MCIM_MUSIC_ENTRY*)allocator(sizeof(MCIM_MUSIC_ENTRY));
  if (entry == NULL) {
    mcim_command_close(id);
    deallocator(path);
    return NULL;
  }
  entry->key = MCIM_NEXT_KEY++;
  entry->id = id;
  entry->status = MCIM_STATUS_LOADED;
  entry->volume = volume;
  entry->filepath = path;
  entry->next = NULL;

  assert(entry->key != MCIM_INVALID_KEY);
  return entry;
}

static bool mcim_entry_is_playing(const MCIM_MUSIC_ENTRY* entry) {
  return (entry->status >= MCIM_STATUS_PLAYING);
}

static bool mcim_entry_equal(const MCIM_MUSIC_ENTRY* entry, const wchar_t* filepath) {
  assert(entry != NULL);
  assert(entry->filepath != NULL);
  assert(filepath != NULL);

  return (wcscmp(entry->filepath, filepath) == 0);
}

static MCIM_NOTIFY_FLAGS mcim_convert_flag(uint32_t mci_flag) {
  MCIM_NOTIFY_FLAGS mcim_flag;
  switch (mci_flag) {
    case MCI_NOTIFY_SUCCESSFUL:
      mcim_flag = MCIM_NOTIFY_SUCCESSFUL;
      break;
    case MCI_NOTIFY_SUPERSEDED:
      mcim_flag = MCIM_NOTIFY_SUPERSEDED;
      break;
    case MCI_NOTIFY_ABORTED:
      mcim_flag = MCIM_NOTIFY_ABORTED;
      break;
    case MCI_NOTIFY_FAILURE:
      /* fallthrough */
    default:
      mcim_flag = MCIM_NOTIFY_FAILURE;
      break;
  }
  return mcim_flag;
}

/**************************************************************************************************/

static MCIM_KEY mcim_load_entry(MCIM_MUSIC_ENTRY* entry) {
  assert(entry != NULL);

  if (entry->status == MCIM_STATUS_UNLOADED) {
    MCIDEVICEID id;
    if (!mcim_command_open(&id, entry->filepath)) {
      return MCIM_INVALID_KEY;
    }
    entry->id = id;
    entry->status = MCIM_STATUS_LOADED;
  }

  assert(entry->key != MCIM_INVALID_KEY);
  return entry->key;
}

static bool mcim_unload_entry(MCIM_MUSIC_ENTRY* entry, mcim_deallocator_t deallocator) {
  assert(entry != NULL);
  assert(deallocator != NULL);

  if (entry->status >= MCIM_STATUS_LOADED) {
    if (!mcim_stop_entry(entry, deallocator)) {
      return false;
    }
    if (!mcim_command_close(entry->id)) {
      return false;
    }
    entry->status = MCIM_STATUS_UNLOADED;
  }
  return true;
}

static bool mcim_play_entry(MCIM_MUSIC_ENTRY* entry,
                            int32_t from,
                            MCIM_CALLBACK_PROC callback,
                            HWND callbackWindow,
                            mcim_allocator_t allocator,
                            mcim_deallocator_t deallocator) {
  assert(entry != NULL);
  assert(from >= 0);
  assert(!(from > 0 && callback != NULL));
  assert(allocator != NULL);
  assert(deallocator != NULL);

  if (entry->status >= MCIM_STATUS_PLAYING) {
    return true;
  }

  // mci_command実行 → table更新の順とすることでsupersededメッセージが
  // 元のcallbackに正常に送信されるようにしている
  if (entry->status >= MCIM_STATUS_LOADED) {
    bool result;
    if (callback == NULL) {
      if (from == 0) {
        result = mcim_command_play(entry->id);
      } else {
        result = mcim_command_play_from(entry->id, from);
      }
    } else {
      result = mcim_command_play_callback(entry->id, callbackWindow);
    }
    if (result) {
      entry->status = MCIM_STATUS_PLAYING;
      if (!mcim_add_callback_table(entry->id, callback, allocator)) {
        mcim_stop_entry(entry, deallocator);
        return false;
      } else {
        return true;
      }
    }
  }
  return false;
}

static bool mcim_stop_entry(MCIM_MUSIC_ENTRY* entry, mcim_deallocator_t deallocator) {
  assert(entry != NULL);
  assert(deallocator != NULL);

  // mci_command実行 → table更新の順とすることでabortedメッセージが
  // 元のcallbackに正常に送信されるようにしている
  if (entry->status >= MCIM_STATUS_PLAYING) {
    if (!mcim_command_stop(entry->id)) {
      return false;
    }
    entry->status = MCIM_STATUS_LOADED;
    mcim_del_callback_table(entry->id, deallocator);
  }
  return true;
}

static bool mcim_fadeout_entry(MCIM_MUSIC_ENTRY* restrict entry,
                               MCIM_THREAD_DATA* restrict th,
                               MCIM_WAIT_NEXT_FRAME wait,
                               int32_t time,
                               MCIM_CALLBACK_PROC callback,
                               mcim_allocator_t allocator,
                               mcim_deallocator_t deallocator) {
  assert(entry != NULL);
  assert(th != NULL);
  assert(wait != NULL);
  assert(time >= 0);
  assert(allocator != NULL);
  assert(deallocator != NULL);

  if (!mcim_start_fadeout_thread(th, entry, wait, time)) {
    return false;
  }

  // fadeoutはmci_commandにはないため、
  // fadeoutの実行を開始しても元のcallbackにabortedが通知されない
  // そのため、手動でabortedメッセージを再現する必要がある
  MCIM_CALLBACK_PROC proc = mcim_find_callback(entry->id);
  if (proc != NULL) {
    proc(MCIM_NOTIFY_ABORTED);
  }

  if (callback == NULL) {
    mcim_del_callback_table(entry->id, deallocator);
  } else {
    if (!mcim_add_callback_table(entry->id, callback, allocator)) {
      mcim_stop_entry(entry, deallocator);
      return false;
    }
  }

  return true;
}

/**************************************************************************************************/

static LRESULT CALLBACK mcim_callback(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION) {
    MSG* msg = (MSG*)lParam;
    if (msg->message == MM_MCINOTIFY) {
      MCIDEVICEID id = (MCIDEVICEID)(msg->lParam);
      MCIM_CALLBACK_PROC callback = mcim_find_callback(id);
      if (callback != NULL) {
        MCIM_NOTIFY_FLAGS flag = mcim_convert_flag((uint32_t)(msg->wParam));
        callback(flag);
        return 0;
      }
    }
  }

  return CallNextHookEx(MCIM_HOOK, code, wParam, lParam);
}

static MCIM_CALLBACK_PROC mcim_find_callback(MCIDEVICEID id) {
  MCIM_CALLBACK_TABLE_ENTRY* item;

  EnterCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
  HASH_FIND_INT(MCIM_CALLBACK_TABLE, (void*)(uintptr_t)id, item);
  LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
  if (item == NULL) {
    return NULL;
  }
  return item->callback;
}

static bool mcim_add_callback_table(MCIDEVICEID id, MCIM_CALLBACK_PROC callback, mcim_allocator_t allocator) {
  assert(allocator != NULL);

  if (callback == NULL) {
    return true;
  }

  MCIM_CALLBACK_TABLE_ENTRY* item;
  EnterCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
  HASH_FIND_INT(MCIM_CALLBACK_TABLE, (void*)(uintptr_t)id, item);
  if (item != NULL) {
    item->callback = callback;
    LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
    return true;
  } else {
    item = (MCIM_CALLBACK_TABLE_ENTRY*)allocator(sizeof(MCIM_CALLBACK_TABLE_ENTRY));
    if (item == NULL) {
      LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
      return false;
    }
    item->id = id;
    item->callback = callback;
    HASH_ADD_INT(MCIM_CALLBACK_TABLE, id, item);
    LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
    return true;
  }
}

static void mcim_del_callback_table(MCIDEVICEID id, mcim_deallocator_t deallocator) {
  assert(deallocator != NULL);

  MCIM_CALLBACK_TABLE_ENTRY* item;
  EnterCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
  HASH_FIND_INT(MCIM_CALLBACK_TABLE, (void*)(uintptr_t)id, item);
  if (item != NULL) {
    HASH_DEL(MCIM_CALLBACK_TABLE, item);
    LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
    deallocator(item);
  } else {
    LeaveCriticalSection(&MCIM_CALLBACK_TABLE_MUTEX);
  }
}

/**************************************************************************************************/

static bool mcim_command_open(MCIDEVICEID* restrict pId, const wchar_t* restrict filepath) {
  assert(pId != NULL);
  assert(filepath != NULL);

  MCI_OPEN_PARMSW mop = {.lpstrDeviceType = L"MPEGVideo", .lpstrElementName = filepath};

  MCIERROR result = mciSendCommandW(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)(&mop));
  *pId = mop.wDeviceID;
  return (result == 0);
}

static bool mcim_command_get_volume(MCIDEVICEID id, uint32_t* pVolume) {
  assert(pVolume != NULL);

  MCI_STATUS_PARMS msp = {.dwItem = MCI_DGV_STATUS_VOLUME};

  if (mciSendCommandW(id, MCI_STATUS, MCI_WAIT | MCI_DGV_STATUS_NOMINAL | MCI_STATUS_ITEM, (DWORD_PTR)(&msp)) != 0) {
    return false;
  } else {
    *pVolume = msp.dwReturn;
    return true;
  }
}

static bool mcim_command_set_volume(MCIDEVICEID id, uint32_t volume) {
  MCI_DGV_SETAUDIO_PARMSW mdsp = {.dwItem = MCI_DGV_SETAUDIO_VOLUME, .dwValue = volume};

  return (mciSendCommandW(id, MCI_SETAUDIO, MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE, (DWORD_PTR)(&mdsp)) == 0);
}

static bool mcim_command_play(MCIDEVICEID id) {
  return (mciSendCommandW(id, MCI_PLAY, 0, 0) == 0);
}

static bool mcim_command_play_callback(MCIDEVICEID id, HWND callbackWindow) {
  static MCI_PLAY_PARMS mpp = {.dwCallback = 0};
  if (mpp.dwCallback == 0) {
    mpp.dwCallback = (DWORD_PTR)callbackWindow;
  }
  return (mciSendCommandW(id, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)(&mpp)) == 0);
}

static bool mcim_command_play_from(MCIDEVICEID id, int32_t from) {
  assert(from >= 0);

  static const MCI_SET_PARMS msp = {.dwTimeFormat = MCI_FORMAT_MILLISECONDS};
  if (mciSendCommandW(id, MCI_SET, MCI_WAIT | MCI_SET_TIME_FORMAT, (DWORD_PTR)(&msp)) != 0) {
    return false;
  }

  MCI_PLAY_PARMS mpp = {.dwFrom = from};
  return (mciSendCommandW(id, MCI_PLAY, MCI_FROM, (DWORD_PTR)(&mpp)) == 0);
}

static bool mcim_command_stop(MCIDEVICEID id) {
  return (mciSendCommandW(id, MCI_STOP, MCI_WAIT, 0) == 0);
}

static bool mcim_command_close(MCIDEVICEID id) {
  return (mciSendCommandW(id, MCI_CLOSE, MCI_WAIT, 0) == 0);
}

/**************************************************************************************************/

static bool mcim_create_fadeout_thread(MCIM_DATA_INTERNAL* data) {
  (data->th).initializedEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
  if ((data->th).initializedEvent == NULL) {
    return false;
  }

  (data->th).hthread = (HANDLE)_beginthreadex(NULL, 0, mcim_fadeout_thread, data, 0, &((data->th).threadId));
  if ((data->th).hthread == (HANDLE)0) {
    CloseHandle((data->th).initializedEvent);
    return false;
  }

  if (WaitForSingleObject((data->th).initializedEvent, INFINITE) != WAIT_OBJECT_0) {
    mcim_terminate_fadeout_thread(&(data->th));
    return false;
  }

  atomic_init(&((data->th).wait), 0);

  return true;
}

static bool mcim_start_fadeout_thread(MCIM_THREAD_DATA* restrict th,
                                      const MCIM_MUSIC_ENTRY* restrict entry,
                                      MCIM_WAIT_NEXT_FRAME wait,
                                      int32_t time) {
  assert(th != NULL);
  assert(entry != NULL);
  assert(wait != NULL);

  atomic_store(&(th->wait), (uintptr_t)wait);
  return PostThreadMessageW(th->threadId, MCIM_TM_FADEOUT, (WPARAM)entry, (LPARAM)time) != 0;
}

static void mcim_terminate_fadeout_thread(MCIM_THREAD_DATA* th) {
  PostThreadMessageW(th->threadId, MCIM_TM_TERMINATE, 0, 0);
  CloseHandle(th->initializedEvent);
  WaitForSingleObject(th->hthread, INFINITE);
  CloseHandle(th->hthread);
}

static unsigned __stdcall mcim_fadeout_thread(void* pargs) {
  MCIM_DATA_INTERNAL* data = (MCIM_DATA_INTERNAL*)pargs;
  MCIM_CALLBACK_PROC callback = NULL;
  MSG msg;
  PeekMessageW(&msg, (HWND)-1, WM_USER, WM_USER, PM_NOREMOVE);

  SetEvent((data->th).initializedEvent);

  while (GetMessageW(&msg, (HWND)-1, 0, 0) != 0) {
    if (msg.message == MCIM_TM_TERMINATE) {
      break;
    }
    if (msg.message != MCIM_TM_FADEOUT) {
      continue;
    }
    MCIM_MUSIC_ENTRY* entry = (MCIM_MUSIC_ENTRY*)(msg.wParam);
    int32_t time = (int32_t)(msg.lParam);
    int32_t cnt = 0;
    MCIM_WAIT_NEXT_FRAME wait = (MCIM_WAIT_NEXT_FRAME)atomic_load(&((data->th).wait));
    callback = mcim_find_callback(entry->id);

    while (++cnt > time) {
      if (PeekMessageW(&msg, (HWND)-1, 0, 0, PM_REMOVE) == 0 || msg.message == MCIM_TM_TERMINATE) {
        goto finish;
      }
      if (entry->status != MCIM_STATUS_FADINGOUT) {
        break;
      }
      int32_t volume = (int32_t)(1.0 * entry->volume * (time - cnt) / time);
      mcim_command_set_volume(entry->id, volume);
      wait();
    }
    mcim_command_stop(entry->id);
    mcim_command_set_volume(entry->id, entry->volume);
    if (callback != NULL) {
      if (cnt > time) {
        callback(MCIM_NOTIFY_SUCCESSFUL);
      } else {
        callback(MCIM_NOTIFY_ABORTED);
      }
      callback = NULL;
    }
  }

finish:
  if (callback != NULL) {
    callback(MCIM_NOTIFY_ABORTED);
  }

  return 0;
}
