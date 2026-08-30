# STM32 MEMS Mirror Structured-Light Controller

基于 **STM32H743** 的 MEMS 振镜结构光投影同步控制程序。

该工程用于研究期间搭建的单轴 MEMS 振镜结构光三维测量系统。核心功能是捕获 MEMS 振镜输出的同步脉冲，并将其作为每个扫描周期的时间基准；随后通过 STM32 定时器内部触发、Output Compare、DMA 与 DAC，按照预先计算的时间序列控制激光器输出，同时完成相机同步触发。

本仓库为历史项目整理版本，主要用于记录和展示实际使用过的嵌入式同步控制方案，并非面向通用 MEMS 振镜系统设计的控制库。

## 核心功能

- 使用 TIM1 Input Capture 捕获 MEMS 振镜输出的同步脉冲。
- 以 MEMS 同步脉冲作为每个扫描周期的时间起点。
- 通过 TIM1 → TIM2 的内部触发关系建立硬件同步时基。
- 使用 TIM2 Output Compare 按预设时间序列产生周期内定时事件。
- 使用 DMA 自动更新 TIM2 CCR 比较值，实现连续时序输出。
- 使用 TIM2 硬件触发 DAC1，并由 DMA 按对应序列更新 DAC 数据。
- 通过 DAC 输出控制激光器调制，实现结构光投射。
- 使用 TIM8 单脉冲输出产生工业相机同步触发信号。
- 通过 USART1 支持运行控制以及 TIM/DAC 时序数据在线更新。
- 将大型 TIM/DAC DMA 数据缓冲区放置于 STM32H743 AXI SRAM。

## 投影时序原理

主投影时序链路如下：

```text
MEMS 振镜同步脉冲
        │
        ▼
TIM1 Input Capture
捕获扫描周期起点
        │
        │  TRGO / Internal Trigger
        ▼
TIM2 Slave Reset + Trigger
建立当前 MEMS 周期的时间零点
        │
        ▼
TIM2 Counter
周期内连续计时
        │
        ├──────── DMA ────────► 更新下一组 CCR 比较值
        │
        ▼
TIM2 Output Compare Event
到达预设激光输出时刻
        │
        │  TRGO
        ▼
DAC1 Hardware Trigger
        │
        ├──────── DMA ────────► 更新对应 DAC 数据
        │
        ▼
DAC 模拟输出
        │
        ▼
激光器调制
        │
        ▼
结构光投射
```

关键的周期内时序主要由：

```text
Timer + Internal Trigger + Output Compare + DMA + DAC
```

共同完成，而不是依赖 CPU 使用延时函数或在中断中逐点修改 DAC，这样可以降低以下软件因素对投影时序的影响：

- 中断响应延迟；
- 中断抢占；
- HAL / 软件执行时间；
- CPU 当前负载；
- 软件调度抖动。

## 相机同步

相机触发采用独立的 TIM8 单脉冲输出。
因此激光周期内精确定时与相机触发分别由不同定时器完成，便于独立配置投影时序和图像采集时序。

## 开发环境

项目开发环境：

- MCU：STM32H743IITx
- 编辑器：Visual Studio Code
- 外设配置：STM32CubeMX
- 构建系统：CMake + Ninja
- 编译工具链：STM32CubeCLT / GNU Arm Embedded Toolchain
- 底层库：STM32 HAL / CMSIS
- 开发语言：C

当前工程可通过 CMake 独立构建，不依赖特定 IDE 工程文件。

## 工程结构

```text
.
├── App/
│   ├── Assets/              # 初始 TIM / DAC 时序数据
│   ├── Inc/                 # 应用层头文件与配置
│   └── Src/                 # 投影控制与通信逻辑
│
├── Core/                    # STM32CubeMX 生成代码
├── Drivers/                 # STM32 HAL / CMSIS
├── cmake/                   # CMake 工具链与 CubeMX 构建配置
│
├── CMakeLists.txt
├── CMakePresets.json
├── SLprojection_app.ioc     # STM32CubeMX 工程配置
├── STM32H743XX_FLASH.ld
└── startup_stm32h743xx.s
```

应用层主要包括：

### `App/Src/app_projector.c`

负责：

- 投影运行状态管理；
- TIM / DAC DMA 启停；
- 投影时序切换；
- MEMS 周期同步处理；
- 相机触发控制。

### `App/Src/app_commander.c`

负责：

- USART1 命令接收；
- Start / Stop / Restart / Next 等运行控制；
- TIM / DAC 时序数据在线更新。

### `App/Inc/app_config.h`

负责：

- 投影数据最大尺寸；
- 通信参数；
- 相机触发参数；
- DMA 缓冲区配置。

## 内存布局

TIM 与 DAC 的运行时数据量较大，因此 DMA 缓冲区通过自定义 linker section 放置于 STM32H743 的 AXI SRAM：

```c
__attribute__((section(".AXI_SRAM_BUFFERS")))
__attribute__((aligned(32)))
```

当前最大缓冲配置为：

```text
TIM buffer : 100 × 500 × uint32_t = 200000 Bytes
DAC buffer : 100 × 500 × uint32_t = 200000 Bytes

Total      : 400000 Bytes
```

用于保存运行过程中可更新的投影时间序列与 DAC 数据。

## 构建

Debug：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Release：

```bash
cmake --preset Release
cmake --build --preset Release
```

当前工程已在以下工具链下完成构建验证：

```text
STM32CubeCLT
GNU Arm Embedded GCC 14.3.1
CMake
Ninja
```

## 第三方代码

本工程包含 STM32CubeMX 生成代码以及 STM32 HAL、CMSIS 等第三方组件。

其版权和许可遵循 STMicroelectronics / Arm 随源码提供的原始声明。

本仓库主要用于历史项目归档。