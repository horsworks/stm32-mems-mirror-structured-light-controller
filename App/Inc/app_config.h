#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"  // 引入 HAL 库基础定义 (uint32_t 等)

// ==========================================
//  系统参数配置
// ==========================================
#define MAX_ROWS 100  // 数组最大行数
#define MAX_COLS 500  // 数组最大列数
#define TIMEOUT_RX 5000  // 串口通信超时时间

// 相机默认参数
#define CAM_DEFAULT_START    55  // 开始曝光的周期数
#define CAM_DEFAULT_INTERVAL 50  // 曝光间隔周期
#define CAM_DEFAULT_TIMES    1  // 单帧投影采集次数
#define CAM_STOP_OFFSET      CAM_DEFAULT_INTERVAL - 10  // 单帧停止采集后等待周期

// 投影仪默认参数
#define PROJ_AUTO_FLAG 1  // 自动切换标记

// ==========================================
//  内存管理
// ==========================================
// 将大数组放入 AXI SRAM 并按 32 字节对齐，供 DMA 时序缓冲使用。
// 链接脚本中已定义 .AXI_SRAM_BUFFERS 段。
#define DATA_IN_AXI_SRAM  __attribute__((section(".AXI_SRAM_BUFFERS"))) __attribute__((aligned(32)))

#endif // APP_CONFIG_H