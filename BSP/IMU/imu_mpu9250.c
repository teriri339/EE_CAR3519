#include "IMU/imu_mpu9250.h"

/*
 * MPU9250 驱动 - 软件 I2C (GPIO 模拟)
 *
 * 接线:
 *   PA0 → MPU9250 SDA  (需外接 4.7kΩ 上拉到 3.3V)
 *   PA1 → MPU9250 SCL  (需外接 4.7kΩ 上拉到 3.3V)
 *
 * I2C 地址: 0x68 (AD0=GND)
 *
 * 只用到陀螺仪 Z 轴用于循迹 D 项。
 */

/* ========== GPIO 引脚定义 ========== */
#define I2C_PORT        GPIOA
#define I2C_SDA_PIN     DL_GPIO_PIN_0
#define I2C_SCL_PIN     DL_GPIO_PIN_1

/* I2C 时序: ~100kHz @ 80MHz (delay_cycles(400) ≈ 5μs per half-cycle) */
#define I2C_DELAY       delay_cycles(400)

/* ========== SDA/SCL 控制宏 ========== */
#define SDA_LOW()  do { \
    DL_GPIO_clearPins(I2C_PORT, I2C_SDA_PIN); \
    DL_GPIO_enableOutput(I2C_PORT, I2C_SDA_PIN); \
} while(0)

#define SDA_REL()  do { \
    DL_GPIO_disableOutput(I2C_PORT, I2C_SDA_PIN); \
} while(0)

#define SDA_READ() (DL_GPIO_readPins(I2C_PORT, I2C_SDA_PIN) != 0)

#define SCL_H()    DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN)
#define SCL_L()    DL_GPIO_clearPins(I2C_PORT, I2C_SCL_PIN)

/* ========== MPU9250 寄存器地址 ========== */
#define MPU_ADDR        0x68        /* I2C 地址 (AD0=GND) */
#define REG_PWR_MGMT_1  0x6B        /* 电源管理 */
#define REG_GYRO_CONFIG 0x1B        /* 陀螺仪量程 */
#define REG_CONFIG      0x1A        /* DLPF 配置 */
#define REG_WHO_AM_I    0x75        /* 芯片 ID (应为 0x71) */
#define REG_GYRO_ZOUT_H 0x47        /* 陀螺 Z 高字节 */

/* ========== 软件 I2C 基础操作 ========== */

static void i2c_start(void)
{
    SDA_REL();
    I2C_DELAY;
    SCL_H();
    I2C_DELAY;
    SDA_LOW();
    I2C_DELAY;
    SCL_L();
    I2C_DELAY;
}

static void i2c_stop(void)
{
    SDA_LOW();
    I2C_DELAY;
    SCL_H();
    I2C_DELAY;
    SDA_REL();
    I2C_DELAY;
}

static uint8_t i2c_wait_ack(void)
{
    uint8_t ack;

    SDA_REL();
    I2C_DELAY;
    SCL_H();
    I2C_DELAY;
    ack = SDA_READ();
    SCL_L();
    I2C_DELAY;

    return (ack == 0) ? 0 : 1;   /* 0=ACK, 1=NACK */
}

static void i2c_send_ack(uint8_t ack)
{
    if (ack)
        SDA_REL();      /* NACK: 释放总线 */
    else
        SDA_LOW();      /* ACK: 拉低 */

    I2C_DELAY;
    SCL_H();
    I2C_DELAY;
    SCL_L();
    I2C_DELAY;
}

static void i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--)
    {
        if (data & (1 << i))
            SDA_REL();
        else
            SDA_LOW();

        I2C_DELAY;
        SCL_H();
        I2C_DELAY;
        SCL_L();
        I2C_DELAY;
    }
}

static uint8_t i2c_read_byte(uint8_t send_ack)
{
    uint8_t data = 0;

    SDA_REL();
    for (int i = 7; i >= 0; i--)
    {
        I2C_DELAY;
        SCL_H();
        I2C_DELAY;
        if (SDA_READ())
            data |= (1 << i);
        SCL_L();
    }

    i2c_send_ack(send_ack ? 0 : 1);   /* send_ack=1 → ACK; send_ack=0 → NACK */

    return data;
}

/* ========== MPU9250 寄存器读写 ========== */

static int mpu_write_reg(uint8_t reg, uint8_t val)
{
    i2c_start();
    i2c_write_byte(MPU_ADDR << 1);      /* 写地址 */
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_write_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_write_byte(val);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_stop();
    return 0;
}

static int mpu_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    /* 先写寄存器地址 */
    i2c_start();
    i2c_write_byte(MPU_ADDR << 1);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_write_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    /* 重复 START + 读 */
    i2c_start();
    i2c_write_byte((MPU_ADDR << 1) | 1);   /* 读地址 */
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t ack = (i < len - 1) ? 1 : 0;  /* 最后一字节 NACK */
        buf[i] = i2c_read_byte(ack);
    }

    i2c_stop();
    return 0;
}

/* ========== 公开接口 ========== */

int imu_init(void)
{
    uint8_t whoami;

    /* 配置 GPIO: SCL=推挽输出, SDA 初始释放(输入+上拉) */
    DL_GPIO_initDigitalOutputFeatures(IOMUX_PINCM2,   /* PA1=SCL, PINCM2 */
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_enableOutput(I2C_PORT, I2C_SCL_PIN);
    SCL_H();

    /* SDA: 输入模式 (外接上拉, 需要输出时切为开漏 LOW) */
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM1,    /* PA0=SDA, PINCM1 */
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    SDA_REL();

    /* 简单延时等 MPU9250 上电稳定 */
    delay_ms(100);

    /* 验证芯片 ID */
    if (mpu_read_regs(REG_WHO_AM_I, &whoami, 1) != 0)
        return -1;
    if (whoami != 0x71)
        return -2;

    /* 唤醒 (退出睡眠) + 自动选择时钟源 */
    mpu_write_reg(REG_PWR_MGMT_1, 0x01);
    delay_ms(10);

    /* 陀螺仪量程: ±250dps, 灵敏度 131 LSB/(°/s) */
    mpu_write_reg(REG_GYRO_CONFIG, 0x00);

    /* DLPF: 带宽 250Hz, 延迟 0.97ms */
    mpu_write_reg(REG_CONFIG, 0x00);

    return 0;
}

int16_t imu_get_gyro_z(void)
{
    uint8_t buf[2];

    if (mpu_read_regs(REG_GYRO_ZOUT_H, buf, 2) != 0)
        return 0;

    /* 有符号 16bit, 大端 */
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);

    return raw;
}
