#ifndef APP_COMMANDER_H
#define APP_COMMANDER_H

#include "app_config.h"
#include "app_projector.h" // 需要控制 Projector
#include "usart.h"         // 需要 UART_HandleTypeDef

typedef struct {
    uint8_t rx_byte;         // 单字节接收缓冲
    uint8_t cmd_buffer[20];  // 字符串命令缓冲
    uint8_t cmd_idx;
    volatile uint8_t stop_request; // 原子标志位
    UART_HandleTypeDef *huart;
} Commander_t;

extern Commander_t g_comm;

void Comm_Init(Commander_t *cmd, UART_HandleTypeDef *huart);

// 串口接收中断回调
void Comm_OnRxISR(Commander_t *cmd);

// 主循环调用的处理任务
void Comm_HandleUpdateProcess(Commander_t *cmd, Projector_t *p);

#endif // APP_COMMANDER_H