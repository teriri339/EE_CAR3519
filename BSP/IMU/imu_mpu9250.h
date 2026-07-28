#ifndef __IMU_MPU9250_H__
#define __IMU_MPU9250_H__

#include <stdint.h>

/* MPU9250 software I2C: PA0=SDA, PA1=SCL, address 0x68. */
int imu_init(void);
int imu_read_gyro_z(int16_t *raw_z);
int imu_calibrate_gyro_z(uint16_t sample_count, uint16_t interval_ms);
int16_t imu_get_gyro_z_bias(void);

#endif
