/**
 * @file allegro.h
 * @author Halkaze
 * @brief Allegro headers reproduction for MCIManager
 * @date 2023-09-13
 *
 * @copyright Copyright (c) 2023
 */

// 正規のallegro v4.0.0.0のAllegro.hは、内部でC++を利用しており、
// 必然的にincludeする側のプログラムもC++とする必要があった。
// MCIManagerはC言語のみで書くライブラリであるから、
// 本ヘッダを用意することでこの問題に対処した。

#include <windows.h>

HWND win_get_window(void);
