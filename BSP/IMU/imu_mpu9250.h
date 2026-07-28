#ifndef __IMU_MPU9250_H__
#define __IMU_MPU9250_H__

#include "bsp.h"

/* 初始化 MPU9250 (软件 I2C, PA0=SDA, PA1=SCL)
 * 返回 0=成功, -1=I2C 通信失败 */
int imu_init(void);

/* 读取陀螺仪 Z 轴原始值 (带符号 16bit)
 * 量程 ±250dps, 灵敏度 131 LSB/(°/s)
 * 返回 raw 值, 0 表示读取失败 */
int16_t imu_get_gyro_z(void);

#endif
