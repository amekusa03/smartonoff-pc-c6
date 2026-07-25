#pragma once
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// 乖離確定時に呼ばれるコールバック。actual_on = GPIO が示す実態。
// コールバック内で Matter 属性を更新し、ユーザーへ通知する。
// システムはこのコールバック内で ON/OFF 操作を行ってはならない。
typedef void (*pc_monitor_mismatch_cb_t)(bool actual_on);

// pc_control_init() の後に呼ぶ。
esp_err_t pc_monitor_init(pc_monitor_mismatch_cb_t mismatch_cb);

// Matter コマンド実行時に呼ぶ。
// want_on = コマンドの期待値。起動・シャットダウン中の過渡期間を抑制する。
void pc_monitor_notify_command(bool want_on);

#ifdef __cplusplus
}
#endif
