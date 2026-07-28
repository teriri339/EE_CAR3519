#include "IMU/imu_mpu9250.h"

/*
 * MPU9250 驱动 - 软件 I2C (DL_GPIO + 直接寄存器混合)
 *
 * 接线:
 *   PA0 → MPU9250 SDA  (需外接 4.7kΩ 上拉到 3.3V)
 *   PA1 → MPU9250 SCL
 *   AD0 → GND
 */

/* ========== 引脚定义 ========== */
#define I2C_PORT        GPIOA
#define I2C_SDA_PIN     DL_GPIO_PIN_0
#define I2C_SCL_PIN     DL_GPIO_PIN_1

/* I2C 延时: ~50kHz */
#define I2C_DELAY       delay_cycles(800)

/* ========== SDA/SCL 控制 (DL_GPIO 函数) ========== */

static void sda_out_low(void)
{
    DL_GPIO_clearPins(I2C_PORT, I2C_SDA_PIN);
    DL_GPIO_enableOutput(I2C_PORT, I2C_SDA_PIN);
}

static void sda_release(void)
{
    DL_GPIO_setPins(I2C_PORT, I2C_SDA_PIN);
    DL_GPIO_disableOutput(I2C_PORT, I2C_SDA_PIN);
}

static uint8_t sda_read(void)
{
    return (DL_GPIO_readPins(I2C_PORT, I2C_SDA_PIN) != 0) ? 1 : 0;
}

static void scl_high(void)  { DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN); }
static void scl_low(void)   { DL_GPIO_clearPins(I2C_PORT, I2C_SCL_PIN); }

/* ========== I2C 协议 ========== */

static void i2c_start(void)
{
    sda_release();
    I2C_DELAY;
    scl_high();
    I2C_DELAY;
    sda_out_low();
    I2C_DELAY;
    scl_low();
    I2C_DELAY;
}

static void i2c_stop(void)
{
    sda_out_low();
    I2C_DELAY;
    scl_high();
    I2C_DELAY;
    sda_release();
    I2C_DELAY;
}

static uint8_t i2c_wait_ack(void)
{
    uint8_t ack;
    sda_release();
    I2C_DELAY;
    scl_high();
    I2C_DELAY;
    ack = sda_read();
    scl_low();
    I2C_DELAY;
    return ack;
}

static void i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--)
    {
        if (data & (1 << i))
            sda_release();
        else
            sda_out_low();
        I2C_DELAY;
        scl_high();
        I2C_DELAY;
        scl_low();
        I2C_DELAY;
    }
}

static uint8_t i2c_read_byte(uint8_t send_ack)
{
    uint8_t data = 0;
    sda_release();
    I2C_DELAY;

    for (int i = 7; i >= 0; i--)
    {
        scl_high();
        I2C_DELAY;
        if (sda_read())
            data |= (1 << i);
        scl_low();
        I2C_DELAY;
    }

    if (send_ack)
        sda_release();
    else
        sda_out_low();
    I2C_DELAY;
    scl_high();
    I2C_DELAY;
    scl_low();
    I2C_DELAY;

    return data;
}

/* ========== I2C 总线复位 ========== */
static void i2c_bus_reset(void)
{
    sda_release();
    I2C_DELAY;
    for (int i = 0; i < 10; i++)
    {
        scl_low();
        I2C_DELAY;
        scl_high();
        I2C_DELAY;
    }
    i2c_stop();
}

/* ========== MPU9250 寄存器 ========== */
#define MPU_ADDR_DEFAULT  0x68
#define MPU_ADDR_ALT      0x69

#define REG_WHO_AM_I      0x75
#define REG_PWR_MGMT_1    0x6B
#define REG_GYRO_CONFIG   0x1B
#define REG_CONFIG        0x1A
#define REG_GYRO_ZOUT_H   0x47

static uint8_t mpu_addr = 0;

/* ========== 寄存器读写 ========== */

static int mpu_write_reg(uint8_t reg, uint8_t val)
{
    if (mpu_addr == 0) return -1;

    i2c_start();
    i2c_write_byte(mpu_addr << 1);
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
    if (mpu_addr == 0) return -1;

    i2c_start();
    i2c_write_byte(mpu_addr << 1);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_write_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_start();
    i2c_write_byte((mpu_addr << 1) | 1);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    for (uint8_t i = 0; i < len; i++)
        buf[i] = i2c_read_byte((i < len - 1) ? 1 : 0);

    i2c_stop();
    return 0;
}

/* ========== I2C 扫描 ========== */

uint8_t imu_scan_i2c(void)
{
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        i2c_start();
        i2c_write_byte(addr << 1);
        uint8_t ack = i2c_wait_ack();
        i2c_stop();
        if (ack == 0)
            return addr;
    }
    return 0;
}

/* ========== 初始化 ========== */

int imu_init(void)
{
    uint8_t whoami;

    /* SCL (PA1): 推挽输出, 初始 HIGH */
    DL_GPIO_initDigitalOutputFeatures(IOMUX_PINCM2,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_HIGH, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN);
    DL_GPIO_enableOutput(I2C_PORT, I2C_SCL_PIN);

    /* SDA (PA0): 推挽输出, 初始释放 */
    DL_GPIO_initDigitalOutputFeatures(IOMUX_PINCM1,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_HIGH, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_setPins(I2C_PORT, I2C_SDA_PIN);
    DL_GPIO_disableOutput(I2C_PORT, I2C_SDA_PIN);   /* 初始: 释放 */

    /* 延时等模块上电 */
    delay_ms(500);

    /* I2C 总线复位 */
    i2c_bus_reset();
    delay_ms(10);

    /* 扫描 I2C 总线找设备 */
    mpu_addr = imu_scan_i2c();
    if (mpu_addr == 0)
    {
        /* 扫描不到, 单独试 0x68 / 0x69 */
        i2c_start();
        i2c_write_byte(MPU_ADDR_DEFAULT << 1);
        if (i2c_wait_ack() == 0) mpu_addr = MPU_ADDR_DEFAULT;
        else
        {
            i2c_start();
            i2c_write_byte(MPU_ADDR_ALT << 1);
            if (i2c_wait_ack() == 0) mpu_addr = MPU_ADDR_ALT;
        }
        i2c_stop();
    }

    if (mpu_addr == 0)
        return -1;   /* 总线上无设备 */

    /* 读 WHO_AM_I */
    if (mpu_read_regs(REG_WHO_AM_I, &whoami, 1) != 0)
        return -2;

    /* 唤醒芯片 */
    if (mpu_write_reg(REG_PWR_MGMT_1, 0x00) != 0)
        return -3;
    delay_ms(100);

    /* 陀螺量程 ±250dps */
    if (mpu_write_reg(REG_GYRO_CONFIG, 0x00) != 0)
        return -4;

    /* DLPF 250Hz */
    if (mpu_write_reg(REG_CONFIG, 0x00) != 0)
        return -5;

    return 0;
}

/* ========== 读取数据 ========== */

int16_t imu_get_gyro_z(void)
{
    uint8_t buf[2];
    if (mpu_read_regs(REG_GYRO_ZOUT_H, buf, 2) != 0)
        return 0;
    return (int16_t)((buf[0] << 8) | buf[1]);
}

uint8_t imu_get_whoami(void)
{
    uint8_t val = 0;
    if (mpu_read_regs(REG_WHO_AM_I, &val, 1) != 0)
        return 0x00;
    return val;
}

uint8_t imu_get_addr(void)
{
    return mpu_addr;
}
