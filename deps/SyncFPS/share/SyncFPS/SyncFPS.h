/**
 * @file SyncFPS.h
 * @author Halkaze
 * @brief SyncFPS 公開ヘッダ
 * @date 2023-09-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __SYNCFPS_H__
#define __SYNCFPS_H__

#include "compat/attrib.h"

#include <stdbool.h>
#include <stdlib.h>

typedef void* (*sync_fps_allocator_t)(size_t);
typedef void (*sync_fps_deallocator_t)(void*);

static const sync_fps_allocator_t SYNC_FPS_DEFAULT_MEMORY_ALLOCATOR = malloc;
static const sync_fps_deallocator_t SYNC_FPS_DEFAULT_MEMORY_DEALLOCATOR = free;

/**
 * @brief SyncFPS用オブジェクト
 */
typedef struct _SYNC_FPS_DATA SYNC_FPS_DATA, *PSYNC_FPS_DATA;

/**
 * @brief 指定されたメモリアロケータを使用してSyncFPS用オブジェクトを初期化
 * @param[in] fps 想定FPS
 * @param[in] allocator オブジェクト割り当てに使用するメモリアロケータ
 * @param[in] deallocator オブジェクト解放に使用するメモリデアロケータ
 * @return SYNC_FPS_DATA* 初期化済みSyncFPS用オブジェクト
 * @note - fpsが0以下の場合は失敗する
 * @note - allocatorがNULLの場合は失敗する
 * @note - deallocatorがNULLの場合は失敗する
 * @note - 失敗時はNULLを返す
 */
ATTRIB_MALLOC SYNC_FPS_DATA* init_sync_fps_al(double fps, sync_fps_allocator_t allocator, sync_fps_deallocator_t deallocator);

/**
 * @brief SyncFPS用オブジェクトを初期化
 * @param[in] fps 想定FPS
 * @return SYNC_FPS_DATA* 初期化済みSyncFPS用オブジェクト
 * @note - fpsが0以下の場合は失敗する
 * @note - 失敗時はNULLを返す
 */
ATTRIB_MALLOC static inline SYNC_FPS_DATA* init_sync_fps(double fps) {
  return init_sync_fps_al(fps, SYNC_FPS_DEFAULT_MEMORY_ALLOCATOR, SYNC_FPS_DEFAULT_MEMORY_DEALLOCATOR);
}

/**
 * @brief 次のフレームまで実行を待機
 * @param[in,out] data init_sync_fps関数の返り値
 * @return bool 成功時true、失敗時false
 * @note - 引数は何度でも使いまわし可能
 * @note - 引数がNULLの場合は失敗する
 * @note - 別スレッドが同一オブジェクトを用いて待機中である場合、該当スレッド処理終了待機後即座にtrueを返す
 */
bool wait_sync_fps(SYNC_FPS_DATA* data);

/**
 * @brief SyncFPS用オブジェクトを解放
 * @param[in,out] data init_sync_fps関数の返り値
 * @return bool 成功時true、失敗時false
 * @note - 同一オブジェクトに対して他のスレッドが待機中であった場合の処理は未定義
 * @note - 引数がNULLであった場合は何もせずtrueを返す
 */
bool free_sync_fps(SYNC_FPS_DATA* data);

#endif  // __SYNCFPS_H__
