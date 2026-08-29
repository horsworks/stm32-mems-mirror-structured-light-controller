#ifndef APP_PROJECTOR_H
#define APP_PROJECTOR_H

#include "app_config.h"
#include "tim.h"
#include "dac.h"

// --- 子类：相机触发器 ---
typedef struct {
    int flag_counter;     // 当前计数
    int start_threshold;  // 开始触发计数
    int interval;         // 触发间隔
    int max_triggers;     // 最高触发次数
    int trigger_count;    // 当前触发次数
    int stop_threshold;   // 停止计数 (用于判断本行是否结束)
} CameraTrigger_t;

// --- 主类：投影仪 ---
typedef struct {
    // 状态属性
    uint16_t cur_rows;
    uint16_t cur_cols;
    int current_step;
    int auto_mode_flag;

    // 硬件句柄依赖 (指针)
    TIM_HandleTypeDef *htim_pwm;   // TIM2
    TIM_HandleTypeDef *htim_trig;  // TIM1
    TIM_HandleTypeDef *htim_pulse; // TIM8
    DAC_HandleTypeDef *hdac;       // DAC1

    // 数据缓冲区指针 (指向 AXI SRAM)
    uint32_t (*tim_data)[MAX_COLS];
    uint32_t (*dac_data)[MAX_COLS];

    // 聚合组件
    CameraTrigger_t camera;
} Projector_t;

// 全局实例声明
extern Projector_t g_proj;

// --- 方法原型 ---
void Projector_Init(Projector_t *p);
void Projector_StartStep(Projector_t *p);
void Projector_Stop(Projector_t *p);
void Projector_Restart(Projector_t *p);
void Projector_NextRow(Projector_t *p);
void Projector_SwitchAutoFlag(Projector_t *p);
int  Projector_IsRowFinished(Projector_t *p);

// 中断服务函数 (ISR)
void Projector_OnTriggerISR(Projector_t *p);

#endif // APP_PROJECTOR_H