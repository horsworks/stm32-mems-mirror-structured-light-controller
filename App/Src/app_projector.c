#include "app_projector.h"
#include <string.h>

// ==========================================
//  静态内存分配 (AXI SRAM)
// ==========================================
DATA_IN_AXI_SRAM uint32_t TIMDMA_Buffer[MAX_ROWS][MAX_COLS];
DATA_IN_AXI_SRAM uint32_t DACDMA_Buffer[MAX_ROWS][MAX_COLS];

// Flash 中的默认数据 (示例，实际根据你的需求填充)
const uint32_t TIM_Default[MAX_ROWS][MAX_COLS] = {
    #include "InitData_Timer.txt"
    // #include "time_test.txt"
    // #include "TimeTest_Timer.txt"
};

const uint32_t DAC_Default[MAX_ROWS][MAX_COLS] = {
    #include "InitData_DAC.txt"
    // #include "dac_test.txt"
    // #include "TimeTest_DAC.txt"
};



// 全局实例定义
Projector_t g_proj;

// ==========================================
//  私有辅助函数
// ==========================================
static void Camera_Reset(CameraTrigger_t *c) {
    c->flag_counter = 0;
    c->trigger_count = 0;
}

// 触发相机
static void Camera_Update(CameraTrigger_t *c, TIM_HandleTypeDef *pulse_tim) {
    // 1. 每次外部信号到来，总周期计数器加 1
    c->flag_counter++;

    // 2. 判断是否处于有效的工作区间内（前缓冲结束 到 后缓冲结束之间）
    if (c->flag_counter >= c->start_threshold && c->flag_counter <= c->stop_threshold) {
        // 计算从开始触发点算起的“相对周期数”
        uint32_t active_cycles = c->flag_counter - c->start_threshold;
        // 3. 双重校验：满足间隔周期 且 未达到最大触发次数
        if ((active_cycles % c->interval == 0) && (c->trigger_count < c->max_triggers)) {
            // 4. 执行触发
            __HAL_TIM_ENABLE(pulse_tim);
            // 5. 更新状态：已触发次数加 1
            c->trigger_count++;
        }
    }
}

// ==========================================
//  公有方法实现
// ==========================================
void Projector_Init(Projector_t *p) {
    // 1. 绑定硬件句柄
    p->htim_pwm = &htim2;
    p->htim_trig = &htim1;
    p->htim_pulse = &htim8;
    p->hdac = &hdac1;

    // 2. 绑定内存地址
    p->tim_data = TIMDMA_Buffer;
    p->dac_data = DACDMA_Buffer;

    // 3. 加载默认数据 (Flash -> SRAM)
    // 注意：这里假设 Default 数组有内容。如果全 0 可省略或用 memset
    memcpy(p->tim_data, TIM_Default, sizeof(TIMDMA_Buffer));
    memcpy(p->dac_data, DAC_Default, sizeof(DACDMA_Buffer));

    // 4. 计算有效行列
    for (p->cur_rows = 0; p->cur_rows < MAX_ROWS; p->cur_rows++) {
        if (TIM_Default[p->cur_rows][0] == 0) break;
    }
    for (p->cur_cols = 0; p->cur_cols < MAX_COLS; p->cur_cols++) {
        if (TIM_Default[0][p->cur_cols] == 0) break;
    }
    if(p->cur_rows == 0) p->cur_rows = 1; // 防止为0
    if(p->cur_cols == 0) p->cur_cols = 1;

    // 5. 初始化状态
    p->current_step = 1;
    p->auto_mode_flag = PROJ_AUTO_FLAG;

    // 6. 初始化相机参数
    p->camera.start_threshold = CAM_DEFAULT_START;
    p->camera.interval = CAM_DEFAULT_INTERVAL;
    p->camera.max_triggers = CAM_DEFAULT_TIMES;

    if (CAM_DEFAULT_TIMES > 0) {
        p->camera.stop_threshold = CAM_DEFAULT_START + (CAM_DEFAULT_TIMES - 1) * CAM_DEFAULT_INTERVAL + CAM_STOP_OFFSET;
    } else {
        p->camera.stop_threshold = CAM_DEFAULT_START + CAM_STOP_OFFSET; // 如果次数设为0，直接走缓冲
    }

    Camera_Reset(&p->camera);
}

void Projector_Stop(Projector_t *p) {
    HAL_TIM_IC_Stop_IT(p->htim_trig, TIM_CHANNEL_1);
    HAL_TIM_OC_Stop_DMA(p->htim_pwm, TIM_CHANNEL_1);
    HAL_DAC_Stop_DMA(p->hdac, DAC1_CHANNEL_1);
}

void Projector_StartStep(Projector_t *p) {
    int safe_idx = (p->current_step > p->cur_rows) ? 0 : (p->current_step - 1);

    // 启动 PWM DMA 输出 (行数据)
    HAL_TIM_OC_Start_DMA(p->htim_pwm, TIM_CHANNEL_1,
                         (uint32_t *)p->tim_data[safe_idx], p->cur_cols);

    HAL_DAC_Start_DMA(p->hdac, DAC1_CHANNEL_1,
                  (uint32_t *)p->dac_data[safe_idx],
                  p->cur_cols, DAC_ALIGN_12B_R);

    Camera_Reset(&p->camera);

    // 启动触发采集 (开始计数)
    HAL_TIM_IC_Start_IT(p->htim_trig, TIM_CHANNEL_1);
}

void Projector_Restart(Projector_t *p) {
    Projector_Stop(p);
    p->current_step = 1;
    Projector_StartStep(p);
}

void Projector_NextRow(Projector_t *p) {
    Projector_Stop(p);
    p->current_step++;

    // 循环播放逻辑
    if (p->current_step > p->cur_rows) {
        p->current_step = 1;
    }

    Projector_StartStep(p);
}

void Projector_SwitchAutoFlag(Projector_t *p) {
    Projector_Stop(p);
    p->auto_mode_flag = !p->auto_mode_flag;

    Projector_StartStep(p);
}


int Projector_IsRowFinished(Projector_t *p) {
    // 判断当前行的相机触发流程是否走完 + 额外缓冲
    return (p->camera.flag_counter >= p->camera.stop_threshold + 5);
}

// 此函数在 TIM1 CC 中断中调用，要求高效
void Projector_OnTriggerISR(Projector_t *p) {
    // 1. 更新相机逻辑
    Camera_Update(&p->camera, p->htim_pulse);

    // 2. 启动 DAC 传输 (配合 PWM)
    // int safe_idx = (p->current_step > p->cur_rows) ? 0 : (p->current_step - 1);
    //
    // HAL_DAC_Start_DMA(p->hdac, DAC1_CHANNEL_1,
    //                   (uint32_t *)p->dac_data[safe_idx],
    //                   p->cur_cols, DAC_ALIGN_12B_R);
}