#include "GraySensor/gray_sensor.h"

/*
 * 灰度传感器 UART 协议 (CTL_M_TRACK_CH32V006)
 *
 * 请求帧: A5 CMD [ARGS...]
 * 响应帧: 5A CMD STATUS [PAYLOAD...]
 *
 * READ_GRAY8(0x05):
 *   发送: A5 05
 *   响应: 5A 05 00 <sample_counter(1B)> <gray0_l> <gray0_h> ... <gray7_l> <gray7_h>
 *         共 20 字节
 *
 * 灰度值: 每路 uint16_t, 小端格式, 范围 0~4095
 */

#define GRAY_UART   UART_4_INST

/* 接收缓冲区 */
static uint8_t rx_buf[24];

/* 接收超时 (尝试次数) */
#define POLL_TIMEOUT  50000

void gray_sensor_init(void)
{
    /* UART4 已在 SYSCFG_DL_init() 中通过 SYSCFG_DL_UART_4_init() 完成初始化 */
    /* 这里只需要确保 TX 输出使能 */
    DL_GPIO_enableOutput(GPIO_UART_4_TX_PORT, GPIO_UART_4_TX_PIN);
}

void gray_sensor_send_req(uint8_t cmd, const uint8_t *args, uint8_t arg_len)
{
    /* 等待上次发送完成 */
    while (DL_UART_Main_isBusy(GRAY_UART)) {}

    /* 发送帧头 A5 */
    DL_UART_Main_transmitData(GRAY_UART, REQ_HEAD);
    while (DL_UART_Main_isBusy(GRAY_UART)) {}

    /* 发送命令码 */
    DL_UART_Main_transmitData(GRAY_UART, cmd);
    while (DL_UART_Main_isBusy(GRAY_UART)) {}

    /* 发送参数 */
    for (uint8_t i = 0; i < arg_len; i++)
    {
        DL_UART_Main_transmitData(GRAY_UART, args[i]);
        while (DL_UART_Main_isBusy(GRAY_UART)) {}
    }
}

int gray_sensor_recv_rsp(uint8_t *buf, uint8_t expected_len, uint32_t timeout)
{
    uint8_t received = 0;

    while (received < expected_len)
    {
        if (!DL_UART_Main_isRXFIFOEmpty(GRAY_UART))
        {
            buf[received++] = DL_UART_Main_receiveData(GRAY_UART);
        }
        else
        {
            if (timeout == 0)
                break;
            timeout--;
        }
    }

    return (received >= expected_len) ? (int)received : -1;
}

/*
 * 快速读取8路灰度
 * 返回值: sample_counter, <0 表示失败
 */
int gray_sensor_read_all(uint16_t *values)
{
    int ret;

    /* 清空 RX FIFO 遗留数据 */
    while (!DL_UART_Main_isRXFIFOEmpty(GRAY_UART))
    {
        (void)DL_UART_Main_receiveData(GRAY_UART);
    }

    /* 发送 A5 05 */
    gray_sensor_send_req(CMD_READ_GRAY8, NULL, 0);

    /* 接收 20 字节响应 */
    ret = gray_sensor_recv_rsp(rx_buf, RSP_GRAY8_LEN, POLL_TIMEOUT);
    if (ret < 0)
        return -1;

    /* 验证响应帧头 */
    if (rx_buf[0] != RSP_HEAD || rx_buf[1] != CMD_READ_GRAY8 || rx_buf[2] != 0x00)
        return -2;

    /* 解析: rx_buf[3] = sample_counter, rx_buf[4..19] = 8路灰度值 (小端, uint16) */
    for (int i = 0; i < GRAY_CHANNELS; i++)
    {
        uint8_t lo = rx_buf[4 + i * 2];
        uint8_t hi = rx_buf[5 + i * 2];
        values[i] = (uint16_t)((hi << 8) | lo);
    }

    return (int)rx_buf[3];
}

/*
 * 读取单字节寄存器
 * 发送: A5 02 reg
 * 响应: 5A 02 00 reg val
 * 返回值: 0=成功, -1=超时, -2=帧格式错误
 */
int gray_sensor_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t args[1] = {reg};
    int ret;

    /* 清空 RX FIFO */
    while (!DL_UART_Main_isRXFIFOEmpty(GRAY_UART))
    {
        (void)DL_UART_Main_receiveData(GRAY_UART);
    }

    /* 发送 A5 02 reg */
    gray_sensor_send_req(CMD_READ_REG, args, 1);

    /* 接收 5 字节响应: 5A 02 00 reg val */
    ret = gray_sensor_recv_rsp(rx_buf, 5, POLL_TIMEOUT);
    if (ret < 0)
        return -1;

    if (rx_buf[0] != RSP_HEAD || rx_buf[1] != CMD_READ_REG || rx_buf[2] != 0x00)
        return -2;

    *val = rx_buf[4];
    return 0;
}
