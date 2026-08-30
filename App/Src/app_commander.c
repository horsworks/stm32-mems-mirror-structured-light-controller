#include "app_commander.h"
#include <stdio.h>
#include <string.h>

Commander_t g_comm;

void Comm_Init(Commander_t *cmd, UART_HandleTypeDef *huart) {
    cmd->huart = huart;
    cmd->cmd_idx = 0;
    cmd->stop_request = 0;
    memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));

    // 启动中断接收
    HAL_UART_Receive_IT(cmd->huart, &cmd->rx_byte, 1);
}

void Comm_OnRxISR(Commander_t *cmd) {
    // 将接收到的字符存入 buffer
    cmd->cmd_buffer[cmd->cmd_idx++] = cmd->rx_byte;
    if (cmd->cmd_idx >= 18) cmd->cmd_idx = 0;
    cmd->cmd_buffer[cmd->cmd_idx] = 0; // 保持字符串结尾

    // 简单命令解析
    if (strstr((char *)cmd->cmd_buffer, "restart") != NULL) {
        Projector_Restart(&g_proj);

        cmd->cmd_idx = 0;
        memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));
    }
    else if (strstr((char *)cmd->cmd_buffer, "start") != NULL) {
        cmd->stop_request = 0;

        cmd->cmd_idx = 0;
        memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));
    }
    else if (strstr((char *)cmd->cmd_buffer, "stop") != NULL) {
        cmd->stop_request = 1;

        cmd->cmd_idx = 0;
        memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));
    }
    else if (strstr((char *)cmd->cmd_buffer, "next") != NULL) {
        Projector_NextRow(&g_proj);

        cmd->cmd_idx = 0;
        memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));
    }
    else if (strstr((char *)cmd->cmd_buffer, "switch") != NULL) {
        Projector_SwitchAutoFlag(&g_proj);

        cmd->cmd_idx = 0;
        memset(cmd->cmd_buffer, 0, sizeof(cmd->cmd_buffer));
    }

    // 继续接收
    HAL_UART_Receive_IT(cmd->huart, &cmd->rx_byte, 1);
}

void Comm_HandleUpdateProcess(Commander_t *cmd, Projector_t *p) {
    // 如果没有停止请求，直接返回
    if (!cmd->stop_request) return;

    uint8_t update_success = 0;
    // --- 进入在线更新流程 (阻塞式) ---

    // 1. 停止工作
    HAL_UART_AbortReceive(cmd->huart); // 暂停中断接收，防止干扰
    Projector_Stop(p);
    cmd->stop_request = 0;

    // 2. 发送握手信号
    HAL_UART_Transmit(cmd->huart, (uint8_t *)"STOP_ACK\n", 9, 100);
    // 清除 DAC 可能的错误标志
    __HAL_DAC_CLEAR_FLAG(p->hdac, DAC_FLAG_DMAUDR1);
    __HAL_UART_FLUSH_DRREGISTER(cmd->huart);

    // 3. 接收尺寸 [Rows, Cols] (4 Bytes)
    uint16_t dims[2] = {0};
    if (HAL_UART_Receive(cmd->huart, (uint8_t *)dims, 4, 20000) != HAL_OK) {
         HAL_UART_Transmit(cmd->huart, (uint8_t *)"TIMEOUT\n", 8, 100);
         goto EXIT_UPDATE;
    }

    uint16_t new_rows = dims[0];
    uint16_t new_cols = dims[1];

    if (new_rows == 0 || new_rows > MAX_ROWS ||
        new_cols == 0 || new_cols > MAX_COLS) {
        HAL_UART_Transmit(cmd->huart, (uint8_t *)"ERR_SIZE\n", 9, 100);
        goto EXIT_UPDATE;
    }
    HAL_UART_Transmit(cmd->huart, (uint8_t *)"SIZE_ACK\n", 9, 100);

    // 4. 接收数据
    uint32_t row_bytes = new_cols * sizeof(uint32_t);

    // 接收 TIM 数据
    for (int i = 0; i < new_rows; i++) {
        // 直接写入 Projector 的内存指针
        if (HAL_UART_Receive(cmd->huart, (uint8_t *)p->tim_data[i], row_bytes, TIMEOUT_RX) != HAL_OK) {
            HAL_UART_Transmit(cmd->huart, (uint8_t *)"ERR_TIM\n", 8, 100);
            goto EXIT_UPDATE;
        }
    }
    HAL_UART_Transmit(cmd->huart, (uint8_t *)"TIM_ACK\n", 8, 100);

    // 接收 DAC 数据
    for (int i = 0; i < new_rows; i++) {
        if (HAL_UART_Receive(cmd->huart, (uint8_t *)p->dac_data[i], row_bytes, TIMEOUT_RX) != HAL_OK) {
            HAL_UART_Transmit(cmd->huart, (uint8_t *)"ERR_DAC\n", 8, 100);
            goto EXIT_UPDATE;
        }
    }

    // 5. 更新成功，修改参数
    p->cur_rows = new_rows;
    p->cur_cols = new_cols;
    update_success = 1;

    HAL_UART_Transmit(cmd->huart, (uint8_t *)"ALL_DONE\n", 9, 100);

EXIT_UPDATE:
    // 6. 恢复串口接收；仅在完整更新成功后重新启动投影
    __HAL_UART_FLUSH_DRREGISTER(cmd->huart);
    Comm_Init(cmd, cmd->huart); // 重新开启中断接收

    if (update_success) {
        Projector_Restart(p);
    }
}