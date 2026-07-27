# Key_Motor_Demo

## 芯片信息
- **型号**: MSPM0G3519
- **内核**: Cortex-M0+
- **封装**: LQFP-80(PN)
- **Flash**: 512 KB
- **RAM**: 128 KB
- **主频**: 80 MHz
- **SDK**: 2.08.00.03

## 硬件固定引脚
| 外设 | 引脚 | 说明 |
|------|------|------|
| OLED | PB3 | SPI0 SCLK | 硬件固定 |
| OLED | PB2 | SPI0 MOSI | 硬件固定 |
| OLED | PC9 | SPI0 CS | 硬件固定 |
| OLED | PC8 | SPI0 DC | 硬件固定 |
| OLED | PB23 | SPI0 RES | 硬件固定 |
| Keyboard | PB6 | 行线 H1 | 硬件固定 |
| Keyboard | PB7 | 行线 H2 | 硬件固定 |
| Keyboard | PB8 | 行线 H3 | 硬件固定 |
| Keyboard | PB9 | 行线 H4 | 硬件固定 |
| Keyboard | PB20 | 列线 V1 | 硬件固定 |
| Keyboard | PB24 | 列线 V2 | 硬件固定 |
| Keyboard | PB25 | 列线 V3 | 硬件固定 |
| Keyboard | PB27 | 列线 V4 | 硬件固定 |
| Debug | PA20 | SWCLK | 系统保留 |
| Debug | PA19 | SWDIO | 系统保留 |
| Clock | PA5 | HFXIN 40MHz | 系统保留 |
| Clock | PA6 | HFXOUT | 系统保留 |

## 用户外设
| 外设 | 引脚 | 说明 |
|------|------|------|
| _待添加_ | | |

## 编译与烧录
### 编译: 打开 `Project\Key_Motor_Demo.uvprojx`，F7
### Flash: CMSIS-DAP is preconfigured in `Project\Key_Motor_Demo.uvoptx`; build first, then use F8 or `uv4 -f`.

此工程由 **mspm0g3519 skill** 生成。
