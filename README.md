# Key_Motor_Demo - MSPM0G3519 智能小车

## 芯片信息
- **型号**: MSPM0G3519
- **内核**: Cortex-M0+
- **封装**: LQFP-80(PN)
- **Flash**: 512 KB
- **RAM**: 128 KB
- **主频**: 80 MHz
- **SDK**: 2.08.00.03

---

## 文件结构

```
Key_Motor_Demo/
├── User/
│   ├── main.c                    # 主程序：按键处理、循迹循环、OLED显示
│   ├── ti_msp_dl_config.h        # SysConfig生成：引脚/外设宏定义（勿手动改）
│   ├── ti_msp_dl_config.c        # SysConfig生成：外设初始化（勿手动改）
│   └── bsp.h                     # 公共头文件聚合
├── BSP/
│   ├── MotorCtrl/
│   │   ├── motor_ctrl.h          # 电机接口 (MotorId_t, motor_set_speed)
│   │   └── motor_ctrl.c          # DRV8870 IN/IN 模式 PWM 驱动
│   ├── KeyBoard/
│   │   ├── keyboard.h
│   │   └── keyboard.c            # 4×4 矩阵键盘行扫描
│   ├── GraySensor/
│   │   ├── gray_sensor.h         # UART 协议定义
│   │   └── gray_sensor.c         # UART4 通信、8路灰度读取
│   ├── LineFollow/
│   │   ├── line_follow.h         # PID 参数宏与接口
│   │   └── line_follow.c         # 加权位置 + PID 差速循迹
│   └── OLED/
│       ├── spi0_oled.h           # OLED 宏 (SIZE=12, 6×8小字体)
│       ├── spi0_oled.c           # SH1106 128×64 驱动
│       └── spi0_oledfont.h       # 字库
└── Source/ti/                    # TI SDK (driverlib等)
```

---

## 完整引脚映射

### 电机驱动 - DRV8870 ×2 (IN/IN 模式, TIMA0 PWM)

| PWM | MCU引脚 | 驱动板 | TIMA0 | 说明 |
|-----|---------|--------|-------|------|
| PWM1 | **PC2** | M1-IN1 | CC0 | 左轮速度 (PWM调速) |
| PWM2 | **PC3** | M1-IN2 | - (GPIO) | 左轮方向 (0=正/1=反) |
| PWM3 | **PC4** | M2-IN1 | CC1 | 右轮速度 (PWM调速) |
| PWM4 | **PC5** | M2-IN2 | - (GPIO) | 右轮方向 (0=正/1=反) |

TIMA0: 1MHz 时钟, period=1000 → 1kHz PWM

### DRV8870 控制真值表

| IN1 | IN2 | 效果 |
|-----|-----|------|
| PWM(duty) | 0 | 正转（调速） |
| 0 | 1 | 反转（全速，无PWM） |
| 0 | 0 | 停止 |
| 1 | 1 | 刹车 |

> 当前限制：IN2 为 GPIO，反转方向无 PWM 调速。后续如需双向 PWM 可使用 TIMG0 CCP1(PC3) + TIMG14 CCP3(PC5) 扩展。

### 矩阵键盘 4×4 (GPIO 行扫描)

| 信号 | MCU引脚 | 说明 |
|------|---------|------|
| H1~H4 | PB6~PB9 | 行线，推挽输出 |
| V1~V4 | PB20, PB24, PB25, PB27 | 列线，上拉输入 |

按键物理布局：`[1][2][3][A] / [4][5][6][B] / [7][8][9][C] / [*][0][#][D]`

### 灰度传感器 ×8 (UART4)

| 信号 | MCU引脚 | 参数 |
|------|---------|------|
| TX | PB10 | 115200 baud, 8N1 |
| RX | PB11 | |

协议：请求 `A5 05` (2字节)，响应 `5A 05 00 <sample_cnt(1B)> <8×uint16小端>` (20字节)。灰度值 0~4095，白色高、黑色低。

### OLED SH1106 128×64 (SPI0)

| 信号 | MCU引脚 |
|------|---------|
| SPI_SCLK | PB3 |
| SPI_PICO | PB2 |
| CS | PC9 |
| DC | PC8 |
| RES | PB23 |

字体：6×8 小字体 (`#define SIZE 12`)，8行可用。

### 系统保留引脚（勿动）

| 功能 | 引脚 |
|------|------|
| SWCLK | PA20 |
| SWDIO | PA19 |
| HFXIN | PA5 |
| HFXOUT | PA6 |

---

## 按键功能

### 手动控制模式（默认）

| 按键 | 功能 |
|------|------|
| **2** | 前进（两轮同向正转） |
| **8** | 后退（两轮同向反转） |
| **4** | 左旋（M1正 / M2反） |
| **6** | 右旋（M1反 / M2正） |
| **1** | 减速 -100 |
| **3** | 加速 +100 |
| **#** | 速度重置为 500 |
| **5** | 停止 |
| **0** | 紧急停止 |
| **A** | 读取灰度值（显示在 OLED） |
| **B** | 进入 / 退出 循迹模式 |
| **\*** | 返回主菜单 |

### 循迹模式

| 按键 | 功能 |
|------|------|
| **B** | 退出循迹 |
| **\*** | 退出（回主菜单） |
| **0** | 退出并紧急停止 |
| **A** | 切换 PID参数显示 / 正常循迹显示 |
| **1 / 3** | KI -- / ++ |
| **2 / 8** | KD -- / ++ |
| **4 / 6** | KP -10 / +10 |
| **7 / 9** | 基础速度 -50 / +50 |
| **5** | 停止电机 |

---

## 循迹算法 (line_follow.c)

### 流程
1. **二值化**：`ADC < 2000` → 黑线(1)，否则 → 白色(0)
2. **加权位置**：`pos = Σ(bit_i × i) / Σ(bit_i)`
   - 全白或全黑时默认 `pos = 3.5`（中心，直行）
3. **误差**：`error = pos - 3.5`（左负右正）
4. **PID 输出**：`pid = KP×error + KI×∫error + KD×Δerror`
5. **差速**：`M1 = base + pid`，`M2 = base - pid`

### 默认参数

```c
LF_DEFAULT_KP  = 120     // 比例系数
LF_DEFAULT_KI  = 0       // 积分系数（未启用）
LF_DEFAULT_KD  = 0       // 微分系数（未启用）
LF_DEFAULT_SPD = 500     // 基础速度 (0~1000)
BLACK_TH       = 2000    // 黑线二值化阈值
INTEGRAL_LIMIT = 100     // 积分限幅
```

### 已知可优化点
- 目前纯 P 控制 (KI=0, KD=0)，转弯可能震荡。需实测调 KP，弯道加入 KD 抑制过冲
- 二值化阈值 `BLACK_TH=2000` 需根据场地光照校准
- 反转方向无 PWM 调速。如需双向精细调速，可为 PC3/PC5 配置独立 TIMG PWM 通道

---

## 编译与烧录

### 编译

```powershell
D:\EE\UV4\UV4.exe -b "D:\desktop\3519car\Key_Motor_Demo\Key_Motor_Demo.uvprojx"
```

或在 Keil MDK 中打开 `.uvprojx` → F7。

### 烧录

CMSIS-DAP 已预配置，编译后 F8 下载。

---

## 修复记录

| 日期 | 问题 | 修复 |
|------|------|------|
| 2026-07-27 | M1/M2 引脚映射互换 (CC0↔CC1, PH1↔PH2) | 修正 motor_ctrl.c 中 PWM 通道和 GPIO 分配 |
| 2026-07-27 | M2 物理反转 | 交换 M2 电机两端接线（硬件修复，代码不做取反） |
| 2026-07-27 | 循迹模式按5键不停车 | main.c case '5' 添加 motor_set_speed(..., 0) |
| 2026-07-27 | OLED 字太大、排版乱 | SIZE 16→12, 修正小字体页偏移, 重新设计 8行布局 |
| 2026-07-27 | 灰度传感器通信超时 | 波特率 9600→115200, 协议修正为 A5/5A 帧格式, UART API 修正 |
| 2026-07-27 | 主菜单第1行溢出 (18字符>16) | 方向键拆两行: "2:Fwd 8:Rev" + "4:Left 6:Right", 统一16字符对齐 |
| 2026-07-27 | 5键不停车、调速不生效 | 添加方向追踪 apply_direction()，调速后即时重设电机；所有停止路径统一复位 DIR_STOP |
| 2026-07-27 | 去抖导致按键错乱 (5→前进,8→停止) | 回退键盘去抖，保留原始单次扫描 |
| 2026-07-27 | 5全速前进/8停止(PWM极性bug) | CC=0 在 ACOND 下输出 ~100% 而非 0%；停止/反转改用 CC=period(1000) |
| 2026-07-27 | speed越大越慢 + 按8刹车（PWM Down-Counting bug） | PWM 是 Down-Counting Edge-Aligned，占空比=(period-CC)/period；前进 CC=period-speed，停止/反转 CC=999（≈0%），CC=1000 导致永不匹配→IN1=1→刹车 |

---

此工程由 **mspm0g3519 skill** 辅助生成。
