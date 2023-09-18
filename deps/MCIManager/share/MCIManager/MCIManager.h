/**
 * @file MCIManager.h
 * @author Halkaze
 * @brief MCIManager 公開ヘッダ
 * @date 2023-09-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __MCIMANAGER_H__
#define __MCIMANAGER_H__

#include "compat/attrib.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

typedef void* (*mcim_allocator_t)(size_t);
typedef void (*mcim_deallocator_t)(void*);

static const mcim_allocator_t MCIM_DEFAULT_MEMORY_ALLOCATOR = malloc;
static const mcim_deallocator_t MCIM_DEFAULT_MEMORY_DEALLOCATOR = free;

/**
 * @brief MCIManager用オブジェクト
 * @note - 一つのオブジェクトが同時に管理できるBGMは一つのみ
 */
typedef struct _MCIM_DATA MCIM_DATA, *PMCIM_DATA;

/**
 * @brief BGM識別キー
 */
typedef uint32_t MCIM_KEY;

static const MCIM_KEY MCIM_INVALID_KEY = 0xffffffff;
static const MCIM_KEY MCIM_MASTER_KEY = 0xfffffffe;

/**
 * @brief コールバック時の結果を示すフラグ
 */
typedef enum _MCIM_NOTIFY_FLAGS {
  MCIM_NOTIFY_SUCCESSFUL = MCI_NOTIFY_SUCCESSFUL,
  MCIM_NOTIFY_SUPERSEDED = MCI_NOTIFY_SUPERSEDED,
  MCIM_NOTIFY_ABORTED = MCI_NOTIFY_ABORTED,
  MCIM_NOTIFY_FAILURE = MCI_NOTIFY_FAILURE
} MCIM_NOTIFY_FLAGS;

/**
 * @brief コールバック関数のテンプレート
 */
typedef void (*MCIM_CALLBACK_PROC)(MCIM_NOTIFY_FLAGS flag);

/**
 * @brief 待機関数のテンプレート
 */
typedef void (*MCIM_WAIT_NEXT_FRAME)(void);

/**
 * @brief 指定されたメモリアロケータを使用してMCIMオブジェクトを初期化
 * @param[in] callbackWindow BGM再生時のコールバック先ウィンドウ
 * @param[in] allocator オブジェクト割り当てに使用するメモリアロケータ
 * @param[in] deallocator オブジェクト解放に使用するメモリデアロケータ
 * @return MCIM_DATA* 初期化済みMCIMオブジェクト
 * @note - 失敗時はNULLを返す
 * @note - callbackWindowがINVALID_HANDLE_VALUEの場合は失敗する
 * @note - allocatorがNULLの場合は失敗する
 * @note - deallocatorがNULLの場合は失敗する
 */
ATTRIB_MALLOC MCIM_DATA* mcim_init_al(HWND callbackWindow, void* (*allocator)(size_t), void (*deallocator)(void*));

/**
 * @brief MCIMオブジェクトを初期化
 * @param[in] callbackWindow BGM再生時のコールバック先ウィンドウ
 * @return MCIM_DATA* 初期化済みMCIMオブジェクト
 * @note - 失敗時はNULLを返す
 * @note - callbackWindowがINVALID_HANDLE_VALUEの場合は失敗する
 */
ATTRIB_MALLOC static inline MCIM_DATA* mcim_init(HWND callbackWindow) {
  return mcim_init_al(callbackWindow, MCIM_DEFAULT_MEMORY_ALLOCATOR, MCIM_DEFAULT_MEMORY_DEALLOCATOR);
}

/**
 * @brief MCIMオブジェクトを解放
 * @param[in,out] data mcim_initの返り値
 * @return bool 成功時true、失敗時false
 * @note - play中の場合は自動でstopする
 * @note - unloadされていない場合は自動でunlaodする
 * @note - dataがNULLであった場合は何もせずtrueを返す
 * @note - 失敗時にはMCIM_DATAの状態およびメモリ解放状況は不定となる
 */
bool mcim_exit(MCIM_DATA* data);

/**
 * @brief BGMファイルをロード
 *
 * @param[in,out] data mcim_initの返り値
 * @param[in] filepath ロードするBGMファイルのパス
 * @return MCIM_KEY 失敗時にはMCIM_INVALID_KEYを返す
 * @note - 同名ファイルを既にロード済みである場合は失敗する
 * @note - dataがNULLであった場合は失敗する
 * @note - filepathがNULLまたは空であった場合は失敗する
 */
MCIM_KEY mcim_load(MCIM_DATA* data, const wchar_t* filepath);

/**
 * @brief ロード済みBGMをアンロード
 * @param[in,out] data mcim_initの返り値
 * @param[in] key mcim_loadの返り値
 * @return bool 成功時true、失敗時false
 * @note - keyに対応するBGMを再生中の場合は自動でstopする
 * @note - dataがNULLであった場合は失敗する
 * @note - keyに対応するBGMが存在しない場合は失敗する
 * @note - keyに対応するBGMがunload済みの場合は何もせず成功する
 */
bool mcim_unload(MCIM_DATA* data, MCIM_KEY key);

/**
 * @brief ロード済みBGMを再生
 * @param[in,out] data mcim_initの返り値
 * @param[in] key mcim_loadの返り値
 * @param[in] callback 再生終了後に呼ぶコールバック関数
 * @return bool 成功時true、失敗時false
 * @note - 再生は非同期で行われ、再生終了を待たずリターンする
 * @note - dataがNULLであった場合は失敗する
 * @note - keyに対応するBGMがloadされていない場合は失敗する
 * @note - keyに対応するBGMが再生中の場合は何もせず成功する
 * @note - callbackがNULLであった場合はコールバックなしでの再生を試みる
 */
bool mcim_play(MCIM_DATA* data, MCIM_KEY key, MCIM_CALLBACK_PROC callback);

/**
 * @brief ロード済みBGMを指定位置から再生
 * @param[in,out] data mcim_initの返り値
 * @param[in] key mcim_loadの返り値
 * @param[in] from 再生開始位置（ミリ秒単位）
 * @return bool 成功時true、失敗時false
 * @note - 再生は非同期で行われ、再生終了を待たずリターンする
 * @note - dataがNULLであった場合は失敗する
 * @note - keyに対応するBGMがloadされていない場合は失敗する
 * @note - keyに対応するBGMが再生中の場合は何もせず成功する
 * @note - fromが負数の場合は0として扱う
 * @note - fromがBGMの総再生時間よりも長い場合の動作は不定
 */
bool mcim_play_from(MCIM_DATA* data, MCIM_KEY key, int32_t from);

/**
 * @brief 再生中のBGMを即座に停止
 * @param[in,out] data mcim_initの返り値
 * @param[in] key mcim_loadの返り値またはMCIM_MASTER_KEY
 * @return MCIM_KEY 成功時停止したBGMのMCIM_KEYを、失敗時MCIM_INVALID_KEYを返す
 * @note - keyにMCIM_MASTER_KEYを指定することで現在再生中のBGMの停止を試みる
 * @note - dataがNULLであった場合は失敗する
 * @note - keyに対応するBGMが存在しない場合は失敗する
 * @note - keyに対応するBGMが再生中でない場合は何もせず成功する
 */
MCIM_KEY mcim_stop(MCIM_DATA* data, MCIM_KEY key);

/**
 * @brief 再生中のBGMを指定時間かけてフェードアウト
 * @param[in,out] data mcim_initの返り値
 * @param[in] wait 次のフレームまでの待機に用いる関数
 * @param[in] key mcim_loadの返り値またはMCIM_MASTER_KEY
 * @param[in] time 停止までの時間（フレーム単位）
 * @param[in] callback 停止後に呼ぶコールバック関数
 * @return MCIM_KEY 成功時フェードアウトさせたBGMのMCIM_KEYを、失敗時MCIM_INVALID_KEYを返す
 * @note - フェードアウトは非同期で行われ、フェードアウト終了を待たずリターンする
 * @note - keyにMCIM_MASTER_KEYを指定することで現在再生中のBGMのフェードアウトを試みる
 * @note - dataがNULLであった場合は失敗する
 * @note - waitがNULLであった場合は失敗する
 * @note - keyに対応するBGMが存在しない場合は失敗する
 * @note - keyに対応するBGMが再生中でない場合は失敗する
 * @note - timeが負数の場合は失敗する
 * @note - callbackがNULLであった場合はコールバックなしでのフェードアウトを試みる
 */
MCIM_KEY mcim_fadeout(MCIM_DATA* data, MCIM_KEY key, MCIM_WAIT_NEXT_FRAME wait, int32_t time, MCIM_CALLBACK_PROC callback);

#endif  // __MCIMANAGER_H__
