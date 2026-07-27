#ifndef __GRAY_SENSOR_H__
#define __GRAY_SENSOR_H__

#include "bsp.h"

#define GRAY_CHANNELS  8     /* 8路灰度传感器 */

/* UART 协议帧头 */
#define REQ_HEAD   0xA5     /* 请求帧头 */
#define RSP_HEAD   0x5A     /* 响应帧头 */

/* UART 命令码 */
#define CMD_READ_REG    0x02  /* 读寄存器: A5 02 reg */
#define CMD_WRITE_REG   0x03  /* 写寄存器: A5 03 reg val */
#define CMD_READ_GRAY8  0x05  /* 快速读取8路灰度: A5 05 */

/* 寄存器地址 */
#define REG_PROTO_VER     0x00
#define REG_DEVICE_ID     0x01
#define REG_HEARTBEAT     0x02
#define REG_SAMPLE_CNT    0x03
#define REG_GRAY_BITS     0x04  /* 灰度比较位图 */
#define REG_UART_PUSH     0x15  /* 主动发送模式 */

/* 读灰度命令的响应长度：
 * HEAD(1) + CMD(1) + STATUS(1) + sample_counter(1) + 8路灰度(16) = 20 字节 */
#define RSP_GRAY8_LEN  20

/* 初始化 UART4 (PB10-TX, PB11-RX, 115200 8N1) */
void gray_sensor_init(void);

/* 发送请求帧: A5 CMD [ARGS...] */
void gray_sensor_send_req(uint8_t cmd, const uint8_t *args, uint8_t arg_len);

/* 接收响应帧，返回接收到的字节数，-1 表示超时 */
int gray_sensor_recv_rsp(uint8_t *buf, uint8_t expected_len, uint32_t timeout_ms);

/* 快速读取8路灰度值 (values 为 uint16_t[8], 返回采样计数值, -1 表示失败) */
int gray_sensor_read_all(uint16_t *values);

/* 读取单字节寄存器 (如 0x04 灰度位图) */
int gray_sensor_read_reg(uint8_t reg, uint8_t *val);

#endif
