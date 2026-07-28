#include "bsp.h"
#include "IMU/imu_mpu9250.h"

#define IMU_PORT        GPIOA
#define IMU_SDA_PIN     DL_GPIO_PIN_0
#define IMU_SCL_PIN     DL_GPIO_PIN_1
#define IMU_SDA_IOMUX   IOMUX_PINCM1
#define IMU_SCL_IOMUX   IOMUX_PINCM2
#define I2C_DELAY       delay_cycles(400)

#define MPU_ADDR         0x68
#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_GYRO_ZOUT_H  0x47
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

static int16_t gyro_z_bias;

/* Both lines are open-drain: output mode only ever drives low. */
#define SDA_LOW() do { \
    DL_GPIO_clearPins(IMU_PORT, IMU_SDA_PIN); \
    DL_GPIO_enableOutput(IMU_PORT, IMU_SDA_PIN); \
} while (0)
#define SDA_RELEASE() DL_GPIO_disableOutput(IMU_PORT, IMU_SDA_PIN)
#define SDA_READ() (DL_GPIO_readPins(IMU_PORT, IMU_SDA_PIN) != 0U)
#define SCL_LOW() do { \
    DL_GPIO_clearPins(IMU_PORT, IMU_SCL_PIN); \
    DL_GPIO_enableOutput(IMU_PORT, IMU_SCL_PIN); \
} while (0)
#define SCL_RELEASE() DL_GPIO_disableOutput(IMU_PORT, IMU_SCL_PIN)

static void i2c_start(void)
{
    SDA_RELEASE();
    SCL_RELEASE();
    I2C_DELAY;
    SDA_LOW();
    I2C_DELAY;
    SCL_LOW();
}

static void i2c_stop(void)
{
    SDA_LOW();
    I2C_DELAY;
    SCL_RELEASE();
    I2C_DELAY;
    SDA_RELEASE();
    I2C_DELAY;
}

static int i2c_wait_ack(void)
{
    int nack;

    SDA_RELEASE();
    I2C_DELAY;
    SCL_RELEASE();
    I2C_DELAY;
    nack = SDA_READ() ? 1 : 0;
    SCL_LOW();
    I2C_DELAY;
    return nack;
}

static void i2c_send_ack(int ack)
{
    if (ack)
        SDA_LOW();
    else
        SDA_RELEASE();

    I2C_DELAY;
    SCL_RELEASE();
    I2C_DELAY;
    SCL_LOW();
    I2C_DELAY;
    SDA_RELEASE();
}

static void i2c_write_byte(uint8_t data)
{
    int bit;

    for (bit = 7; bit >= 0; bit--)
    {
        if ((data & (1U << bit)) != 0U)
            SDA_RELEASE();
        else
            SDA_LOW();

        I2C_DELAY;
        SCL_RELEASE();
        I2C_DELAY;
        SCL_LOW();
    }
}

static uint8_t i2c_read_byte(int ack)
{
    uint8_t data = 0;
    int bit;

    SDA_RELEASE();
    for (bit = 7; bit >= 0; bit--)
    {
        I2C_DELAY;
        SCL_RELEASE();
        I2C_DELAY;
        if (SDA_READ())
            data |= (uint8_t)(1U << bit);
        SCL_LOW();
    }
    i2c_send_ack(ack);
    return data;
}

static int mpu_write_reg(uint8_t reg, uint8_t value)
{
    i2c_start();
    i2c_write_byte((uint8_t)(MPU_ADDR << 1));
    if (i2c_wait_ack()) { i2c_stop(); return -1; }
    i2c_write_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }
    i2c_write_byte(value);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }
    i2c_stop();
    return 0;
}

static int mpu_read_regs(uint8_t reg, uint8_t *data, uint8_t length)
{
    uint8_t i;

    i2c_start();
    i2c_write_byte((uint8_t)(MPU_ADDR << 1));
    if (i2c_wait_ack()) { i2c_stop(); return -1; }
    i2c_write_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return -1; }

    i2c_start();
    i2c_write_byte((uint8_t)((MPU_ADDR << 1) | 1U));
    if (i2c_wait_ack()) { i2c_stop(); return -1; }
    for (i = 0; i < length; i++)
        data[i] = i2c_read_byte(i + 1U < length);
    i2c_stop();
    return 0;
}

int imu_init(void)
{
    uint8_t who_am_i = 0;

    DL_GPIO_initDigitalInputFeatures(IMU_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IMU_SCL_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    SDA_RELEASE();
    SCL_RELEASE();
    delay_ms(100);

    if (mpu_read_regs(REG_WHO_AM_I, &who_am_i, 1) != 0)
        return -1;
    if (who_am_i != 0x71U)
        return -2;

    if (mpu_write_reg(REG_PWR_MGMT_1, 0x80) != 0)
        return -3;
    delay_ms(100);
    if (mpu_write_reg(REG_PWR_MGMT_1, 0x01) != 0)
        return -3;
    if (mpu_write_reg(REG_GYRO_CONFIG, 0x00) != 0)
        return -3; /* +/-250 dps, 131 LSB/(degree/s). */
    if (mpu_write_reg(REG_CONFIG, 0x03) != 0)
        return -3; /* 41 Hz gyro low-pass filter. */
    if (mpu_write_reg(REG_SMPLRT_DIV, 0x09) != 0)
        return -3; /* 100 Hz sample rate. */

    gyro_z_bias = 0;
    return 0;
}

int imu_read_gyro_z(int16_t *raw_z)
{
    uint8_t data[2];

    if (raw_z == 0 || mpu_read_regs(REG_GYRO_ZOUT_H, data, 2) != 0)
        return -1;
    *raw_z = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    return 0;
}

int imu_calibrate_gyro_z(uint16_t sample_count, uint16_t interval_ms)
{
    int32_t sum = 0;
    int16_t raw;
    uint16_t i;

    if (sample_count == 0U)
        return -1;
    for (i = 0; i < sample_count; i++)
    {
        if (imu_read_gyro_z(&raw) != 0)
            return -1;
        sum += raw;
        delay_ms(interval_ms);
    }
    gyro_z_bias = (int16_t)(sum / sample_count);
    return 0;
}

int16_t imu_get_gyro_z_bias(void)
{
    return gyro_z_bias;
}
